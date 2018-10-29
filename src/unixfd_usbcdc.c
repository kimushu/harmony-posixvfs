#include <unixfd.h>
#include <errno.h>
#include <osal/osal.h>
#include <termios.h>
#include "unixfd_config.h"
#include "system_config.h"
#include "system_definitions.h"
#include "usb/usb_device_cdc.h"

#define READ_PENDING        0xffff

enum {
    USBCDC_STATE_INIT,
    USBCDC_STATE_WAIT_CONFIGURATION,
    USBCDC_STATE_READY,
};

typedef struct {
    uint8_t read_buffer[512] __attribute__(( aligned(16) ));

    uint8_t state;
    uint8_t cdcIndex;

    volatile uint8_t configured     : 1;
    volatile uint8_t read_complete  : 1;
    volatile uint8_t write_pending  : 1;
    uint8_t dtr             : 1;
    uint8_t carrier         : 1;

    uint16_t read_position;
    uint16_t read_length;
    USB_DEVICE_CDC_TRANSFER_HANDLE readTransferHandle;
    USB_DEVICE_HANDLE deviceHandle;
    USB_CDC_LINE_CODING lineCoding;
    OSAL_SEM_DECLARE(ready_sem);
    OSAL_SEM_DECLARE(read_sem);
    OSAL_SEM_DECLARE(write_sem);
    OSAL_MUTEX_DECLARE(read_lock);
    OSAL_MUTEX_DECLARE(write_lock);
} usbcdc_instance_t;

static usbcdc_instance_t unixfd_usbcdc;

static USB_DEVICE_CDC_EVENT_RESPONSE usbcdc_cdc_event_handler(
    USB_DEVICE_CDC_INDEX index,
    USB_DEVICE_CDC_EVENT event,
    void *pData,
    usbcdc_instance_t *instance
) {
    switch (event) {
    case USB_DEVICE_CDC_EVENT_GET_LINE_CODING:
        USB_DEVICE_ControlSend(
            instance->deviceHandle,
            &instance->lineCoding,
            sizeof(instance->lineCoding)
        );
        break;

    case USB_DEVICE_CDC_EVENT_SET_LINE_CODING:
        USB_DEVICE_ControlReceive(
            instance->deviceHandle,
            &instance->lineCoding,
            sizeof(instance->lineCoding)
        );
        break;

    case USB_DEVICE_CDC_EVENT_SET_CONTROL_LINE_STATE:
        {
            USB_CDC_CONTROL_LINE_STATE *pState = (USB_CDC_CONTROL_LINE_STATE *)pData;
            instance->dtr = pState->dtr;
            instance->carrier = pState->carrier;
        }
        /* no-break */

    case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_RECEIVED:
    case USB_DEVICE_CDC_EVENT_SEND_BREAK:
        USB_DEVICE_ControlStatus(instance->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
        break;

    case USB_DEVICE_CDC_EVENT_CONTROL_TRANSFER_DATA_SENT:
        break;

    case USB_DEVICE_CDC_EVENT_READ_COMPLETE:
        if (instance->read_position == READ_PENDING) {
            USB_DEVICE_CDC_EVENT_DATA_READ_COMPLETE *pReadComplete = (USB_DEVICE_CDC_EVENT_DATA_READ_COMPLETE *)pData;
            instance->read_length = pReadComplete->length;
            instance->read_complete = 1;
            OSAL_SEM_Post(&instance->read_sem);
        }
        break;

    case USB_DEVICE_CDC_EVENT_WRITE_COMPLETE:
        if (instance->write_pending) {
            instance->write_pending = 0;
            OSAL_SEM_Post(&instance->write_sem);
        }
        break;
    }

    return USB_DEVICE_CDC_EVENT_RESPONSE_NONE;
}

static void usbcdc_device_event_handler(USB_DEVICE_EVENT event, void *eventData, usbcdc_instance_t *instance)
{
    switch (event) {
    case USB_DEVICE_EVENT_RESET:
        instance->configured = 0;
        break;

    case USB_DEVICE_EVENT_CONFIGURED:
        {
            USB_DEVICE_EVENT_DATA_CONFIGURED *pConfigured = (USB_DEVICE_EVENT_DATA_CONFIGURED *)eventData;
            if (pConfigured->configurationValue == 1) {
                instance->cdcIndex = UNIXFD_USBCDC_DEVICE_CDC_INDEX;
                USB_DEVICE_CDC_EventHandlerSet(
                    UNIXFD_USBCDC_DEVICE_CDC_INDEX,
                    (USB_DEVICE_CDC_EVENT_HANDLER)usbcdc_cdc_event_handler,
                    (uintptr_t)instance
                );
                instance->configured = 1;
            }
            break;
        }
        break;
    }
}

static int usbcdc_open(unixfd_fd_t *fd, const char *pathname, int flags, mode_t mode)
{
    fd->private = &unixfd_usbcdc;
    return 0;
}

static ssize_t usbcdc_read(unixfd_fd_t *fd, void *buf, size_t count)
{
    usbcdc_instance_t *instance = (usbcdc_instance_t *)fd->private;
    ssize_t result;

    if ((((fd->flags & O_ACCMODE) + 1) & 1) == 0) {
        return -EACCES;
    }

    while (!instance->configured) {
        if (fd->flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        OSAL_SEM_Pend(&instance->ready_sem, OSAL_WAIT_FOREVER);
    }

    if (fd->flags & O_NONBLOCK) {
        if (OSAL_MUTEX_Lock(&instance->read_lock, 0) == OSAL_RESULT_FALSE) {
            return -EAGAIN;
        }
    } else if (OSAL_MUTEX_Lock(&instance->read_lock, OSAL_WAIT_FOREVER) == OSAL_RESULT_FALSE) {
        return -EDEADLK;
    }

    for (;;) {
        while (instance->read_position == READ_PENDING) {
            if (instance->read_complete) {
                instance->read_position = 0;
                instance->read_complete = 0;
                SYS_DEVCON_DataCacheInvalidate((uintptr_t)instance->read_buffer, sizeof(instance->read_buffer));
                break;
            } else if (fd->flags & O_NONBLOCK) {
                result = -EAGAIN;
                goto aborted;
            }
            OSAL_SEM_Pend(&instance->read_sem, OSAL_WAIT_FOREVER);
        }

        result = instance->read_length - instance->read_position;
        if (result > 0) {
            if (result > count) {
                result = count;
            }
            memcpy(buf, instance->read_buffer + instance->read_position, result);
            instance->read_position += result;
        }

        if (instance->read_length >= instance->read_position) {
            USB_DEVICE_CDC_RESULT cdcResult;
            instance->read_position = READ_PENDING;
            instance->read_complete = 0;
            cdcResult = USB_DEVICE_CDC_Read(instance->cdcIndex, &instance->readTransferHandle,
                instance->read_buffer, sizeof(instance->read_buffer));
            if (cdcResult != USB_DEVICE_CDC_RESULT_OK) {
                instance->read_position = instance->read_length;
                result = -EIO;
                break;
            }
        }

        if (result != 0) {
            break;
        } else if (fd->flags & O_NONBLOCK) {
            result = -EAGAIN;
            break;
        }
    }

aborted:
    OSAL_MUTEX_Unlock(&instance->read_lock);
    return result;
}

static ssize_t usbcdc_write(unixfd_fd_t *fd, const void *buf, size_t count)
{
    USB_DEVICE_CDC_RESULT cdcResult;
    usbcdc_instance_t *instance = (usbcdc_instance_t *)fd->private;
    USB_DEVICE_CDC_TRANSFER_HANDLE transferHandle;
    char alignedBuf[64] __attribute__(( aligned(4) ));

    if ((((fd->flags & O_ACCMODE) + 1) & 2) == 0) {
        return -EACCES;
    }

    while (!instance->configured) {
        if (fd->flags & O_NONBLOCK) {
            return -EAGAIN;
        }
        OSAL_SEM_Pend(&instance->ready_sem, OSAL_WAIT_FOREVER);
    }

    if (fd->flags & O_NONBLOCK) {
        if (OSAL_MUTEX_Lock(&instance->write_lock, 0) == OSAL_RESULT_FALSE) {
            return -EAGAIN;
        }
    } else if (OSAL_MUTEX_Lock(&instance->write_lock, OSAL_WAIT_FOREVER) == OSAL_RESULT_FALSE) {
        return -EDEADLK;
    }
    
    if (((uintptr_t)buf) & 3) {
        if (count > sizeof(alignedBuf)) {
            count = sizeof(alignedBuf);
        }
        memcpy(alignedBuf, buf, count);
        buf = alignedBuf;
    }

    SYS_DEVCON_DataCacheClean((uintptr_t)buf, count);
    instance->write_pending = 1;
    transferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
    cdcResult = USB_DEVICE_CDC_Write(instance->cdcIndex, &transferHandle, buf, count, USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
    if (cdcResult == USB_DEVICE_CDC_RESULT_OK) {
        while (instance->write_pending) {
            OSAL_SEM_Pend(&instance->write_sem, OSAL_WAIT_FOREVER);
        }
    }
    OSAL_MUTEX_Unlock(&instance->write_lock);

    return (cdcResult == USB_DEVICE_CDC_RESULT_OK) ? count : -EIO;
}

static int usbcdc_ioctl(unixfd_fd_t *fd, int request, char *argp)
{
    usbcdc_instance_t *instance = (usbcdc_instance_t *)fd->private;

    switch (request) {
    case TIOCMGET:
        *(int *)argp = (instance->dtr ? TIOCM_DTR : 0) | (instance->carrier ? TIOCM_CAR : 0);
        return 0;
    }

    return -EINVAL;
}

UNIXFD_MOUNTPOINT_FILE(mp_dev_usbcdc, "/dev/usb_cdc",
    .open = usbcdc_open,
    .read = usbcdc_read,
    .write = usbcdc_write,
    .ioctl = usbcdc_ioctl
);

static void unixfd_task_usbcdc_instance(usbcdc_instance_t *instance)
{
    switch (instance->state) {
    case USBCDC_STATE_INIT:
        instance->deviceHandle = USB_DEVICE_Open(UNIXFD_USBCDC_DEVICE_INDEX, DRV_IO_INTENT_READWRITE);
        if (instance->deviceHandle == USB_DEVICE_HANDLE_INVALID) {
            break;
        }
        USB_DEVICE_EventHandlerSet(instance->deviceHandle, (USB_DEVICE_EVENT_HANDLER)usbcdc_device_event_handler, (uintptr_t)instance);
        USB_DEVICE_Attach(instance->deviceHandle);
        instance->state = USBCDC_STATE_WAIT_CONFIGURATION;
        break;

    case USBCDC_STATE_WAIT_CONFIGURATION:
        if (instance->configured) {
            instance->state = USBCDC_STATE_READY;
        }
        break;

    case USBCDC_STATE_READY:
        while (OSAL_SEM_GetCount(&instance->ready_sem) == 0) {
            OSAL_SEM_Post(&instance->ready_sem);
        }
        break;
    }
}

void unixfd_task_usbcdc(void)
{
    unixfd_task_usbcdc_instance(&unixfd_usbcdc);
}

static void unixfd_init_usbcdc_instance(usbcdc_instance_t *instance)
{
    instance->state = USBCDC_STATE_INIT;
    instance->configured = 0;
    instance->read_complete = 0;
    instance->write_pending = 0;
    instance->dtr = 0;
    instance->carrier = 0;
    instance->read_position = 0;
    instance->read_length = 0;
    instance->deviceHandle = USB_DEVICE_HANDLE_INVALID;
    instance->lineCoding.dwDTERate = UNIXFD_USBCDC_BAUDRATE;
    instance->lineCoding.bCharFormat = 0;
    instance->lineCoding.bParityType = 0;
    instance->lineCoding.bDataBits = 8;
    OSAL_SEM_Create(&instance->ready_sem, OSAL_SEM_TYPE_COUNTING, 255, 0);
    OSAL_SEM_Create(&instance->read_sem, OSAL_SEM_TYPE_COUNTING, 255, 0);
    OSAL_SEM_Create(&instance->write_sem, OSAL_SEM_TYPE_COUNTING, 255, 0);
    OSAL_MUTEX_Create(&instance->read_lock);
    OSAL_MUTEX_Create(&instance->write_lock);
}

int unixfd_init_usbcdc(void)
{
    unixfd_init_usbcdc_instance(&unixfd_usbcdc);
    return unixfd_mount(&mp_dev_usbcdc);
}
