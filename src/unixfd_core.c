#include "unixfd_config.h"
#include "unixfd.h"
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <osal/osal.h>

#define GET_FD(num)     (((num < 0) || (num >= UNIXFD_MAX_FDS)) ? NULL : &unixfd_fds[num])

#if (UNIXFD_MAX_FDS <= 32)
typedef uint32_t unixfd_fdmap_t;
#else
typedef uint64_t unixfd_fdmap_t;
#endif

unixfd_mountpoint_t *unixfd_mp_head;
unixfd_fd_t unixfd_fds[UNIXFD_MAX_FDS];
unixfd_fdmap_t unixfd_fd_free = (((unixfd_fdmap_t)1) << UNIXFD_MAX_FDS) - 8;

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

int unixfd_mount(unixfd_mountpoint_t *mp)
{
    OSAL_CRITSECT_DATA_TYPE lock = OSAL_CRIT_Enter(OSAL_CRIT_TYPE_HIGH);
    mp->prev = NULL;
    mp->next = unixfd_mp_head;
    unixfd_mp_head->prev = mp;
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

int open(const char *pathname, int flags, mode_t mode)
{
    unixfd_mountpoint_t *mp;
    int len;
    int fdnum;
    unixfd_fd_t *fd;
    int result;

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
    fdnum = unixfd_alloc_fd();
    if (fdnum < 0) {
        errno = -EMFILE;
        return -1;
    }
    fd = &unixfd_fds[fdnum];
    if (mp->flags & UNIXFD_MPFLAG_REL_PATH) {
        pathname += len;
    }
    fd->dev = mp->dev;
    fd->flags = flags;
    result = fd->dev->open ? (*fd->dev->open)(fd, pathname, flags, mode) : 0;
    if (result < 0) {
        errno = -result;
        return -1;
    }
    return fdnum;
}

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
