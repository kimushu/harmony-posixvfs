#ifndef _POSIXVFS_CONFIG_H_
#define _POSIXVFS_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define POSIXVFS_MAX_FDS                  ${CONFIG_POSIXVFS_MAX_FDS}

#define POSIXVFS_FUNCTION_OPEN            ${CONFIG_POSIXVFS_FUNCTION_OPEN?then(1,0)}
#define POSIXVFS_FUNCTION_CLOSE           ${CONFIG_POSIXVFS_FUNCTION_CLOSE?then(1,0)}
#define POSIXVFS_FUNCTION_READ            ${CONFIG_POSIXVFS_FUNCTION_READ?then(1,0)}
#define POSIXVFS_FUNCTION_WRITE           ${CONFIG_POSIXVFS_FUNCTION_WRITE?then(1,0)}
#define POSIXVFS_FUNCTION_LSEEK           ${CONFIG_POSIXVFS_FUNCTION_LSEEK?then(1,0)}
#define POSIXVFS_FUNCTION_FSTAT           ${CONFIG_POSIXVFS_FUNCTION_FSTAT?then(1,0)}
#define POSIXVFS_FUNCTION_IOCTL           ${CONFIG_POSIXVFS_FUNCTION_IOCTL?then(1,0)}

#define POSIXVFS_DEVICE_NULL              ${CONFIG_POSIXVFS_DEVICE_NULL?then(1,0)}
#define POSIXVFS_DEVICE_ZERO              ${CONFIG_POSIXVFS_DEVICE_ZERO?then(1,0)}
#define POSIXVFS_DEVICE_USBCDC            ${CONFIG_POSIXVFS_DEVICE_USBCDC?then(1,0)}

<#switch CONFIG_POSIXVFS_STDIN_SOURCE>
  <#case "Dummy (/dev/null)">
#define POSIXVFS_STDIN_DUMMY              1
#define POSIXVFS_STDIN_DEVICE             "/dev/null"
    <#break>
  <#case "USB-CDC">
#define POSIXVFS_STDIN_USBCDC             1
#define POSIXVFS_STDIN_DEVICE             "/dev/usb_cdc"
    <#break>
  <#default>
#define POSIXVFS_STDIN_FREE               1
</#switch>
<#switch CONFIG_POSIXVFS_STDOUT_SOURCE>
  <#case "Dummy (/dev/null)">
#define POSIXVFS_STDOUT_DUMMY             1
#define POSIXVFS_STDOUT_DEVICE            "/dev/null"
    <#break>
  <#case "USB-CDC">
#define POSIXVFS_STDOUT_USBCDC            1
#define POSIXVFS_STDOUT_DEVICE            "/dev/usb_cdc"
    <#break>
  <#default>
#define POSIXVFS_STDOUT_FREE              1
</#switch>
<#switch CONFIG_POSIXVFS_STDERR_SOURCE>
  <#case "Dummy (/dev/null)">
#define POSIXVFS_STDERR_DUMMY             1
#define POSIXVFS_STDERR_DEVICE            "/dev/null"
    <#break>
  <#case "USB-CDC">
#define POSIXVFS_STDERR_USBCDC            1
#define POSIXVFS_STDERR_DEVICE            "/dev/usb_cdc"
    <#break>
  <#default>
#define POSIXVFS_STDERR_FREE              1
</#switch>

<#if CONFIG_POSIXVFS_DEVICE_USBCDC>
#define POSIXVFS_USBCDC_DEVICE_INDEX      0
#define POSIXVFS_USBCDC_DEVICE_CDC_INDEX  0
#define POSIXVFS_USBCDC_BAUDRATE          ${CONFIG_POSIXVFS_USBCDC_BAUDRATE}
</#if>

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* !_POSIXVFS_CONFIG_H_ */