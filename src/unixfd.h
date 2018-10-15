#ifndef _UNIXFD_H_
#define _UNIXFD_H_

#include <unistd.h>
#include <sys/stat.h>

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

#define UNIXFD_MOUNTPOINT_FILE(_var, _path, _dev)   \
    static unixfd_mountpoint_t _var = { \
        .path = _path, \
        .dev = _dev, \
        .flags = UNIXFD_MPFLAG_FILE, \
    }

#define UNIXFD_MOUNTPOINT_DIR(_var, _path, _dev, _flags)  \
    static unixfd_mountpoint_t _var = { \
        .path = _path, \
        .dev = _dev, \
        .flags = _flags, \
    }

#define UNIXFD_MOUNTPOINT_DIR_ABS(_var, _path, _dev) \
    UNIXFD_MOUNTPOINT_DIR(_var, _path, _dev, 0)

#define UNIXFD_MOUNTPOINT_DIR_REL(_var, _path, _dev) \
    UNIXFD_MOUNTPOINT_DIR(_var, _path, _dev, UNIXFD_MPFLAG_REL_PATH)

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* !_UNIXFD_H_ */