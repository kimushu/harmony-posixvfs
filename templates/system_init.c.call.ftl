
    /*** UnixFD Standard I/O Initialization Code ***/
<#if CONFIG_UNIXFD_DEVICE_NULL == true>
    unixfd_init_null();
</#if>
<#if CONFIG_UNIXFD_DEVICE_ZERO == true>
    unixfd_init_zero();
</#if>
<#if CONFIG_UNIXFD_DEVICE_USBCDC == true>
    unixfd_init_usbcdc();
</#if>
<#if CONFIG_UNIXFD_STDIN_SOURCE != "None (Free)">
    unixfd_stdio_redirect(open(UNIXFD_STDIN_DEVICE, O_RDONLY), STDIN_FILENO);
</#if>
<#if CONFIG_UNIXFD_STDOUT_SOURCE != "None (Free)">
    unixfd_stdio_redirect(open(UNIXFD_STDOUT_DEVICE, O_WRONLY), STDOUT_FILENO);
</#if>
<#if CONFIG_UNIXFD_STDERR_SOURCE != "None (Free)">
    unixfd_stdio_redirect(open(UNIXFD_STDERR_DEVICE, O_WRONLY), STDERR_FILENO);
</#if>
