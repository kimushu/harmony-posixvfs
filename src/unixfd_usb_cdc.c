#include <unixfd.h>
#include <errno.h>
#include "unixfd_config.h"
#include "system_config.h"
#include "system_definitions.h"
#include "usb/usb_device_cdc.h"

static ssize_t usb_cdc_read(unixfd_fd_t *fd, void *buf, size_t count)
{
    return -EIO;
}

static ssize_t usb_cdc_write(unixfd_fd_t *fd, const void *buf, size_t count)
{
    return -EIO;
}

UNIXFD_MOUNTPOINT_FILE(mp_dev_usb_cdc, "/dev/usb-cdc",
    .read = usb_cdc_read,
    .write = usb_cdc_write
);

int unixfd_init_usb_cdc(void)
{
    return unixfd_mount(&mp_dev_usb_cdc);
}
