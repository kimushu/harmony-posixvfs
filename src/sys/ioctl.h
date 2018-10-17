#ifndef _SYS_IOCTL_H_
#define _SYS_IOCTL_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int ioctl(int fd, int request, char *argp);

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif  /* _SYS_IOCTL_H_ */
