#ifndef _UNIXFD_H_
#define _UNIXFD_H_

#include <unistd.h>
#include <sys/stat.h>
#include "unixfd_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNIXFD_MPFLAG_PATHLEN_MASK  0x000000ff
#define UNIXFD_MPFLAG_FILE          0x00010000
#define UNIXFD_MPFLAG_REL_PATH      0x00020000

typedef struct
{
    const struct unixfd_device_t *dev;
    void *private;
    int flags;
}
unixfd_fd_t;

typedef struct unixfd_device_t
{
    int     (*open )(unixfd_fd_t *fd, const char *pathname, int flags, mode_t mode);
    int     (*close)(unixfd_fd_t *fd);
    ssize_t (*read )(unixfd_fd_t *fd, void *buf, size_t count);
    ssize_t (*write)(unixfd_fd_t *fd, const void *buf, size_t count);
    off_t   (*lseek)(unixfd_fd_t *fd, off_t offset, int whence);
    int     (*fstat)(unixfd_fd_t *fd, struct stat *buf);
    int     (*ioctl)(unixfd_fd_t *fd, int request, char *argp);
}
unixfd_device_t;

typedef struct unixfd_mountpoint_t
{
    struct unixfd_mountpoint_t *prev, *next;
    const char *path;
    const unixfd_device_t *dev;
    unsigned int flags;
}
unixfd_mountpoint_t;

extern int unixfd_mount(unixfd_mountpoint_t *mp);
extern int unixfd_umount(unixfd_mountpoint_t *mp);
extern int unixfd_stdio_redirect(int old_fdnum, int new_fdnum);

#if (UNIXFD_DEVICE_NULL)
extern int unixfd_init_null(void);
#endif
#if (UNIXFD_DEVICE_ZERO)
extern int unixfd_init_zero(void);
#endif
#if (UNIXFD_DEVICE_USB_CDC)
extern int unixfd_init_usb_cdc(void);
#endif

#define UNIXFD_MOUNTPOINT_FILE(_name, _path, ...)   \
    static const unixfd_device_t _name##_dev = { \
        __VA_ARGS__ \
    }; \
    unixfd_mountpoint_t _name = { \
        .path = _path, \
        .dev = &_name##_dev, \
        .flags = UNIXFD_MPFLAG_FILE, \
    }

#define UNIXFD_MOUNTPOINT_DIR(_name, _path, _flags, ...)  \
    static const unixfd_device_t _name##_dev = { \
        __VA_ARGS__ \
    }; \
    unixfd_mountpoint_t _name = { \
        .path = _path, \
        .dev = &_name##_dev, \
        .flags = _flags, \
    }

#define UNIXFD_MOUNTPOINT_DIR_ABS(_name, _path, ...) \
    UNIXFD_MOUNTPOINT_DIR(_name, _path, 0, __VA_ARGS__)

#define UNIXFD_MOUNTPOINT_DIR_REL(_name, _path, ...) \
    UNIXFD_MOUNTPOINT_DIR(_name, _path, UNIXFD_MPFLAG_REL_PATH, __VA_ARGS__)

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* !_UNIXFD_H_ */