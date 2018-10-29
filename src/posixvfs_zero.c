#include <posixvfs.h>
#include <string.h>
#include "posixvfs_config.h"

static ssize_t zero_read(posixvfs_fd_t *fd, void *buf, size_t count)
{
    memset(buf, 0, count);
    return count;
}

static ssize_t zero_write(posixvfs_fd_t *fd, const void *buf, size_t count)
{
    return count;
}

POSIXVFS_MOUNTPOINT_FILE(mp_dev_zero, "/dev/zero",
    .read = zero_read,
    .write = zero_write
);

int posixvfs_init_zero(void)
{
    return posixvfs_mount(&mp_dev_zero);
}
