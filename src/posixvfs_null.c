#include <posixvfs.h>
#include "posixvfs_config.h"

static ssize_t null_read(posixvfs_fd_t *fd, void *buf, size_t count)
{
    return 0;
}

static ssize_t null_write(posixvfs_fd_t *fd, const void *buf, size_t count)
{
    return count;
}

POSIXVFS_MOUNTPOINT_FILE(mp_dev_null, "/dev/null",
    .read = null_read,
    .write = null_write
);

int posixvfs_init_null(void)
{
    return posixvfs_mount(&mp_dev_null);
}
