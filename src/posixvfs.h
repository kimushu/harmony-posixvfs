#ifndef _POSIXVFS_H_
#define _POSIXVFS_H_

#include <unistd.h>
#include <sys/stat.h>
#include "posixvfs_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POSIXVFS_MPFLAG_PATHLEN_MASK  0x000000ff
#define POSIXVFS_MPFLAG_FILE          0x00010000
#define POSIXVFS_MPFLAG_REL_PATH      0x00020000

typedef struct
{
    const struct posixvfs_device_t *dev;
    void *private;
    int flags;
}
posixvfs_fd_t;

typedef struct posixvfs_device_t
{
    int     (*open )(posixvfs_fd_t *fd, const char *pathname, int flags, mode_t mode);
    int     (*close)(posixvfs_fd_t *fd);
    ssize_t (*read )(posixvfs_fd_t *fd, void *buf, size_t count);
    ssize_t (*write)(posixvfs_fd_t *fd, const void *buf, size_t count);
    off_t   (*lseek)(posixvfs_fd_t *fd, off_t offset, int whence);
    int     (*fstat)(posixvfs_fd_t *fd, struct stat *buf);
    int     (*ioctl)(posixvfs_fd_t *fd, int request, char *argp);
}
posixvfs_device_t;

typedef struct posixvfs_mountpoint_t
{
    struct posixvfs_mountpoint_t *prev, *next;
    const char *path;
    const posixvfs_device_t *dev;
    unsigned int flags;
}
posixvfs_mountpoint_t;

extern int posixvfs_mount(posixvfs_mountpoint_t *mp);
extern int posixvfs_umount(posixvfs_mountpoint_t *mp);
extern int posixvfs_stdio_redirect(int old_fdnum, int new_fdnum);

#if (POSIXVFS_DEVICE_NULL)
extern int posixvfs_init_null(void);
#endif
#if (POSIXVFS_DEVICE_ZERO)
extern int posixvfs_init_zero(void);
#endif
#if (POSIXVFS_DEVICE_USBCDC)
extern int posixvfs_init_usbcdc(void);
extern void posixvfs_task_usbcdc(void);
#endif

#define POSIXVFS_MOUNTPOINT_FILE(_name, _path, ...)   \
    static const posixvfs_device_t _name##_dev = { \
        __VA_ARGS__ \
    }; \
    posixvfs_mountpoint_t _name = { \
        .path = _path, \
        .dev = &_name##_dev, \
        .flags = POSIXVFS_MPFLAG_FILE, \
    }

#define POSIXVFS_MOUNTPOINT_DIR(_name, _path, _flags, ...)  \
    static const posixvfs_device_t _name##_dev = { \
        __VA_ARGS__ \
    }; \
    posixvfs_mountpoint_t _name = { \
        .path = _path, \
        .dev = &_name##_dev, \
        .flags = _flags, \
    }

#define POSIXVFS_MOUNTPOINT_DIR_ABS(_name, _path, ...) \
    POSIXVFS_MOUNTPOINT_DIR(_name, _path, 0, __VA_ARGS__)

#define POSIXVFS_MOUNTPOINT_DIR_REL(_name, _path, ...) \
    POSIXVFS_MOUNTPOINT_DIR(_name, _path, POSIXVFS_MPFLAG_REL_PATH, __VA_ARGS__)

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* !_POSIXVFS_H_ */