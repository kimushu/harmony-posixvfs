#include "unixfd_config.h"
#include "unixfd.h"
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <osal/osal.h>

#define GET_FD(num)     (((num < 0) || (num >= UNIXFD_MAX_FDS)) ? NULL : &unixfd_fds[num])

#if (UNIXFD_MAX_FDS <= 32)
typedef uint32_t unixfd_fdmap_t;
#else
typedef uint64_t unixfd_fdmap_t;
#endif

unixfd_mountpoint_t *unixfd_mp_head;
unixfd_fd_t unixfd_fds[UNIXFD_MAX_FDS];
unixfd_fdmap_t unixfd_fd_free = (((unixfd_fdmap_t)1) << UNIXFD_MAX_FDS) - 1;

static int unixfd_alloc_fd(void)
{
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    int fdnum;
    if (unixfd_fd_free == 0) {
        fdnum = -1;
    } else {
#if (UNIXFD_MAX_FDS <= 32)
        fdnum = _ctz(unixfd_fd_free);
#else
        fdnum = _dctz(unixfd_fd_free);
#endif
        unixfd_fd_free &= ~(((unixfd_fdmap_t)1) << fdnum);
    }
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
    return fdnum;
}

static void unixfd_release_fd(int fdnum)
{
    unixfd_fd_t *fd = &unixfd_fds[fdnum];
    fd->dev = NULL;
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    unixfd_fd_free |= (((unixfd_fdmap_t)1u) << fdnum);
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
}

static int unixfd_open_device_fd(int fdnum, const unixfd_device_t *dev, const char *pathname, int flags, mode_t mode)
{
    unixfd_fd_t *fd = &unixfd_fds[fdnum];
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

static int unixfd_open_device(const unixfd_device_t *dev, const char *pathname, int flags, mode_t mode)
{
    int fdnum;
    int result;

    fdnum = unixfd_alloc_fd();
    if (fdnum < 0) {
        errno = -EMFILE;
        return -1;
    }
    result = unixfd_open_device_fd(fdnum, dev, pathname, flags, mode);
    if (result < 0) {
        unixfd_release_fd(fdnum);
    }
    return result;
}

int unixfd_mount(unixfd_mountpoint_t *mp)
{
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    mp->flags = (mp->flags & ~UNIXFD_MPFLAG_PATHLEN_MASK) | strlen(mp->path);
    mp->prev = NULL;
    mp->next = unixfd_mp_head;
    if (unixfd_mp_head) {
        unixfd_mp_head->prev = mp;
    }
    unixfd_mp_head = mp;
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);
    return 0;
}

int unixfd_umount(unixfd_mountpoint_t *mp)
{
    int result = 0;
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    if (mp->prev) {
        mp->prev->next = mp->next;
    } else if (unixfd_mp_head == mp) {
        unixfd_mp_head = mp->next;
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

int unixfd_stdio_redirect(int old_fdnum, int new_fdnum)
{
    const unixfd_device_t *dev;
    OSAL_CRITSECT_DATA_TYPE lock;
    unixfd_fd_t *old_fd = GET_FD(old_fdnum);
    unixfd_fd_t *new_fd = GET_FD(new_fdnum);

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
    unixfd_fd_free &= ~(((unixfd_fdmap_t)1) << new_fdnum);
    OSAL_CRIT_Leave(OSAL_CRIT_TYPE_HIGH, lock);

    // Replace new_fd with old_fd
    *new_fd = *old_fd;

    // Release old_fd
    unixfd_release_fd(old_fdnum);

    return 0;
}

#if (UNIXFD_FUNCTION_OPEN)
int open(const char *pathname, int flags, mode_t mode)
{
    unixfd_mountpoint_t *mp;
    int len;

    for (mp = unixfd_mp_head; mp; mp = mp->next) {
        len = mp->flags & UNIXFD_MPFLAG_PATHLEN_MASK;
        if (memcmp(pathname, mp->path, len) == 0) {
            if (pathname[len] == '\0') {
                goto found;
            }
            if ((!(mp->flags & UNIXFD_MPFLAG_FILE)) && (pathname[len] == '/')) {
                goto found;
            }
        }
    }
    errno = ENOENT;
    return -1;

found:
    if (mp->flags & UNIXFD_MPFLAG_REL_PATH) {
        pathname += len;
    }
    return unixfd_open_device(mp->dev, pathname, flags, mode);
}
#endif  /* UNIXFD_FUNCTION_OPEN */

#if (UNIXFD_FUNCTION_CLOSE)
int close(int fdnum)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

    if (dev) {
        int result = dev->close ? (*dev->close)(fd) : 0;
        if (result == 0) {
            unixfd_release_fd(fdnum);
            return 0;
        }
        errno = -result;
        return -1;
    }
    errno = EBADF;
    return -1;
}
#endif  /* UNIXFD_FUNCTION_CLOSE */

#if (UNIXFD_FUNCTION_READ)
ssize_t read(int fdnum, void *buf, size_t count)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

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
#endif  /* UNIXFD_FUNCTION_READ */

#if (UNIXFD_FUNCTION_WRITE)
ssize_t write(int fdnum, const void *buf, size_t count)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

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
#endif  /* UNIXFD_FUNCTION_WRITE */

#if (UNIXFD_FUNCTION_LSEEK)
off_t lseek(int fdnum, off_t offset, int whence)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

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
#endif  /* UNIXFD_FUNCTION_LSEEK */

#if (UNIXFD_FUNCTION_FSTAT)
int fstat(int fdnum, struct stat *buf)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

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
#endif  /* UNIXFD_FUNCTION_FSTAT */

#if (UNIXFD_FUNCTION_IOCTL)
int ioctl(int fdnum, int request, char *argp)
{
    unixfd_fd_t *fd = GET_FD(fdnum);
    const unixfd_device_t *dev = fd ? fd->dev : NULL;

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
#endif  /* UNIXFD_FUNCTION_IOCTL */
