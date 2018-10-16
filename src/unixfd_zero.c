#include <unixfd.h>
#include <string.h>
#include "unixfd_config.h"

static ssize_t zero_read(unixfd_fd_t *fd, void *buf, size_t count)
{
    memset(buf, 0, count);
    return count;
}

static ssize_t zero_write(unixfd_fd_t *fd, const void *buf, size_t count)
{
    return count;
}

UNIXFD_MOUNTPOINT_FILE(mp_dev_zero, "/dev/zero",
    .read = zero_read,
    .write = zero_write
);

int unixfd_init_zero(void)
{
    return unixfd_mount(&mp_dev_zero);
}
