#include "posixvfs_config.h"
#include "posixvfs.h"
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <osal/osal.h>

#define GET_FD(num)     (((num < 0) || (num >= POSIXVFS_MAX_FDS)) ? NULL : &posixvfs_fds[num])

#if (POSIXVFS_MAX_FDS <= 32)
typedef uint32_t posixvfs_fdmap_t;
#else
typedef uint64_t posixvfs_fdmap_t;
#endif

posixvfs_mountpoint_t *posixvfs_mp_head;
posixvfs_fd_t posixvfs_fds[POSIXVFS_MAX_FDS];
posixvfs_fdmap_t posixvfs_fd_free = (((posixvfs_fdmap_t)1) << POSIXVFS_MAX_FDS) - 1;

static int posixvfs_alloc_fd(void)
{
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    int fdnum;
    if (posixvfs_fd_free == 0) {
        fdnum = -1;
    } else {
#if (POSIXVFS_MAX_FDS <= 32)
        fdnum = _ctz(posixvfs_fd_free);
#else
        fdnum = _dctz(posixvfs_fd_free);
#endif
        posixvfs_fd_free &= ~(((posixvfs_fdmap_t)1) << fdnum);
    }
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
    return fdnum;
}

static void posixvfs_release_fd(int fdnum)
{
    posixvfs_fd_t *fd = &posixvfs_fds[fdnum];
    fd->dev = NULL;
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    posixvfs_fd_free |= (((posixvfs_fdmap_t)1u) << fdnum);
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
}

static int posixvfs_open_device_fd(int fdnum, const posixvfs_device_t *dev, const char *pathname, int flags, mode_t mode)
{
    posixvfs_fd_t *fd = &posixvfs_fds[fdnum];
    int result;

    fd->dev = dev;
    fd->private = NULL;
    fd->flags = flags;
    result = fd->dev->open ? (*fd->dev->open)(fd, pathname, flags, mode) : 0;
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return fdnum;
}

static int posixvfs_open_device(const posixvfs_device_t *dev, const char *pathname, int flags, mode_t mode)
{
    int fdnum;
    int result;

    fdnum = posixvfs_alloc_fd();
    if (fdnum < 0) {
        errno = -EMFILE;
        return -1;
    }
    result = posixvfs_open_device_fd(fdnum, dev, pathname, flags, mode);
    if (result < 0) {
        posixvfs_release_fd(fdnum);
    }
    return result;
}

int posixvfs_mount(posixvfs_mountpoint_t *mp)
{
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    mp->flags = (mp->flags & ~POSIXVFS_MPFLAG_PATHLEN_MASK) | strlen(mp->path);
    mp->prev = NULL;
    mp->next = posixvfs_mp_head;
    if (posixvfs_mp_head) {
        posixvfs_mp_head->prev = mp;
    }
    posixvfs_mp_head = mp;
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
    return 0;
}

int posixvfs_umount(posixvfs_mountpoint_t *mp)
{
    int result = 0;
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    if (mp->prev) {
        mp->prev->next = mp->next;
    } else if (posixvfs_mp_head == mp) {
        posixvfs_mp_head = mp->next;
    } else {
        result = -1;
        errno = EINVAL;
        goto failed;
    }
    if (mp->next) {
        mp->next->prev = mp->prev;
        mp->next = NULL;
    }
    mp->prev = NULL;
failed:
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
    return result;
}

int posixvfs_stdio_redirect(int old_fdnum, int new_fdnum)
{
    const posixvfs_device_t *dev;
    OSAL_CRITSECT_DATA_TYPE lock;
    posixvfs_fd_t *old_fd = GET_FD(old_fdnum);
    posixvfs_fd_t *new_fd = GET_FD(new_fdnum);

    if ((!old_fd) || (!new_fd) || (new_fdnum >= 3)) {
        errno = EINVAL;
        return -1;
    }
    if (!old_fd->dev) {
        errno = EBADF;
        return -1;
    }
    if (old_fd == new_fd) {
        return 0;
    }

    // Close new_fd (without releasing fd)
    dev = new_fd->dev;
    if (dev) {
        int result = dev->close ? (*dev->close)(new_fd) : 0;
        if (result < 0) {
            return result;
        }
    }

    // Allocate new_fd (if not allocated)
    lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    posixvfs_fd_free &= ~(((posixvfs_fdmap_t)1) << new_fdnum);
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);

    // Replace new_fd with old_fd
    *new_fd = *old_fd;

    // Release old_fd
    posixvfs_release_fd(old_fdnum);

    return 0;
}

#if (POSIXVFS_FUNCTION_OPEN)
int open(const char *pathname, int flags, mode_t mode)
{
    posixvfs_mountpoint_t *mp;
    int len;

    for (mp = posixvfs_mp_head; mp; mp = mp->next) {
        len = mp->flags & POSIXVFS_MPFLAG_PATHLEN_MASK;
        if (memcmp(pathname, mp->path, len) == 0) {
            if (pathname[len] == '\0') {
                goto found;
            }
            if ((!(mp->flags & POSIXVFS_MPFLAG_FILE)) && (pathname[len] == '/')) {
                goto found;
            }
        }
    }
    errno = ENOENT;
    return -1;

found:
    if (mp->flags & POSIXVFS_MPFLAG_REL_PATH) {
        pathname += len;
    }
    return posixvfs_open_device(mp->dev, pathname, flags, mode);
}
#endif  /* POSIXVFS_FUNCTION_OPEN */

#if (POSIXVFS_FUNCTION_CLOSE)
int close(int fdnum)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        int result = dev->close ? (*dev->close)(fd) : 0;
        if (result == 0) {
            posixvfs_release_fd(fdnum);
            return 0;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* POSIXVFS_FUNCTION_CLOSE */

#if (POSIXVFS_FUNCTION_READ)
ssize_t read(int fdnum, void *buf, size_t count)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        ssize_t result = dev->read ? (*dev->read)(fd, buf, count) : -EPERM;
        if (result >= 0) {
            return result;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* POSIXVFS_FUNCTION_READ */

#if (POSIXVFS_FUNCTION_WRITE)
ssize_t write(int fdnum, const void *buf, size_t count)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        ssize_t result = dev->write ? (*dev->write)(fd, buf, count) : -EPERM;
        if (result >= 0) {
            return result;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* POSIXVFS_FUNCTION_WRITE */

#if (POSIXVFS_FUNCTION_LSEEK)
off_t lseek(int fdnum, off_t offset, int whence)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        off_t result = dev->lseek ? (*dev->lseek)(fd, offset, whence) : -ESPIPE;
        if (result >= 0) {
            return result;
        }
        errno = -result;
        return (off_t)-1;
    }
    errno = EBADF;
    return (off_t)-1;
}
#endif  /* POSIXVFS_FUNCTION_LSEEK */

#if (POSIXVFS_FUNCTION_FSTAT)
int fstat(int fdnum, struct stat *buf)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        int result = dev->fstat ? (*dev->fstat)(fd, buf) : -EACCES;
        if (result >= 0) {
            return 0;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* POSIXVFS_FUNCTION_FSTAT */

#if (POSIXVFS_FUNCTION_IOCTL)
int ioctl(int fdnum, int request, char *argp)
{
    posixvfs_fd_t *fd = GET_FD(fdnum);
    const posixvfs_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        int result = dev->ioctl ? (*dev->ioctl)(fd, request, argp) : -EINVAL;
        if (result >= 0) {
            return result;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* POSIXVFS_FUNCTION_IOCTL */
