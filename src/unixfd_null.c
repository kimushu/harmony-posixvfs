#include <unixfd.h>
#include "unixfd_config.h"

static ssize_t null_read(unixfd_fd_t *fd, void *buf, size_t count)
{
    return 0;
}

static ssize_t null_write(unixfd_fd_t *fd, const void *buf, size_t count)
{
    return count;
}

UNIXFD_MOUNTPOINT_FILE(mp_dev_null, "/dev/null",
    .read = null_read,
    .write = null_write
);

int unixfd_init_null(void)
{
    return unixfd_mount(&mp_dev_null);
}
