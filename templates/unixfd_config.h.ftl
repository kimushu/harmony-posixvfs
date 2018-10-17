#ifndef _UNIXFD_CONFIG_H_
#define _UNIXFD_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define UNIXFD_MAX_FDS                  ${CONFIG_UNIXFD_MAX_FDS}

#define UNIXFD_FUNCTION_OPEN            ${CONFIG_UNIXFD_FUNCTION_OPEN?then(1,0)}
#define UNIXFD_FUNCTION_CLOSE           ${CONFIG_UNIXFD_FUNCTION_CLOSE?then(1,0)}
#define UNIXFD_FUNCTION_READ            ${CONFIG_UNIXFD_FUNCTION_READ?then(1,0)}
#define UNIXFD_FUNCTION_WRITE           ${CONFIG_UNIXFD_FUNCTION_WRITE?then(1,0)}
#define UNIXFD_FUNCTION_LSEEK           ${CONFIG_UNIXFD_FUNCTION_LSEEK?then(1,0)}
#define UNIXFD_FUNCTION_FSTAT           ${CONFIG_UNIXFD_FUNCTION_FSTAT?then(1,0)}
#define UNIXFD_FUNCTION_IOCTL           ${CONFIG_UNIXFD_FUNCTION_IOCTL?then(1,0)}

#define UNIXFD_DEVICE_NULL              ${CONFIG_UNIXFD_DEVICE_NULL?then(1,0)}
#define UNIXFD_DEVICE_ZERO              ${CONFIG_UNIXFD_DEVICE_ZERO?then(1,0)}
#define UNIXFD_DEVICE_USBCDC            ${CONFIG_UNIXFD_DEVICE_USBCDC?then(1,0)}

<#switch CONFIG_UNIXFD_STDIN_SOURCE>
  <#case "Dummy (/dev/null)">
#define UNIXFD_STDIN_DUMMY              1
#define UNIXFD_STDIN_DEVICE             "/dev/null"
    <#break>
  <#case "USB-CDC">
#define UNIXFD_STDIN_USBCDC             1
#define UNIXFD_STDIN_DEVICE             "/dev/usb_cdc"
    <#break>
  <#default>
#define UNIXFD_STDIN_FREE               1
</#switch>
<#switch CONFIG_UNIXFD_STDOUT_SOURCE>
  <#case "Dummy (/dev/null)">
#define UNIXFD_STDOUT_DUMMY             1
#define UNIXFD_STDOUT_DEVICE            "/dev/null"
    <#break>
  <#case "USB-CDC">
#define UNIXFD_STDOUT_USBCDC            1
#define UNIXFD_STDOUT_DEVICE            "/dev/usb_cdc"
    <#break>
  <#default>
#define UNIXFD_STDOUT_FREE              1
</#switch>
<#switch CONFIG_UNIXFD_STDERR_SOURCE>
  <#case "Dummy (/dev/null)">
#define UNIXFD_STDERR_DUMMY             1
#define UNIXFD_STDERR_DEVICE            "/dev/null"
    <#break>
  <#case "USB-CDC">
#define UNIXFD_STDERR_USBCDC            1
#define UNIXFD_STDERR_DEVICE            "/dev/usb_cdc"
    <#break>
  <#default>
#define UNIXFD_STDERR_FREE              1
</#switch>

<#if CONFIG_UNIXFD_DEVICE_USBCDC>
#define UNIXFD_USBCDC_DEVICE_INDEX      0
#define UNIXFD_USBCDC_DEVICE_CDC_INDEX  0
#define UNIXFD_USBCDC_BAUDRATE          ${CONFIG_UNIXFD_USBCDC_BAUDRATE}
</#if>

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* !_UNIXFD_CONFIG_H_ */