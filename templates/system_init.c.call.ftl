
    /*** PosixVFS Standard I/O Initialization Code ***/
<#if CONFIG_POSIXVFS_DEVICE_NULL == true>
    posixvfs_init_null();
</#if>
<#if CONFIG_POSIXVFS_DEVICE_ZERO == true>
    posixvfs_init_zero();
</#if>
<#if CONFIG_POSIXVFS_DEVICE_USBCDC == true>
    posixvfs_init_usbcdc();
</#if>
<#if CONFIG_POSIXVFS_STDIN_SOURCE != "None (Free)">
    posixvfs_stdio_redirect(open(POSIXVFS_STDIN_DEVICE, O_RDONLY), STDIN_FILENO);
</#if>
<#if CONFIG_POSIXVFS_STDOUT_SOURCE != "None (Free)">
    posixvfs_stdio_redirect(open(POSIXVFS_STDOUT_DEVICE, O_WRONLY), STDOUT_FILENO);
</#if>
<#if CONFIG_POSIXVFS_STDERR_SOURCE != "None (Free)">
    posixvfs_stdio_redirect(open(POSIXVFS_STDERR_DEVICE, O_WRONLY), STDERR_FILENO);
</#if>
