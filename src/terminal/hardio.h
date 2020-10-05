#ifndef EL__TERMINAL_HARDIO_H
#define EL__TERMINAL_HARDIO_H

#ifdef __cplusplus
extern "C" {
#endif

ssize_t hard_write(int fd, unsigned char *data, size_t datalen);
ssize_t hard_read(int fd, unsigned char *data, size_t datalen);

#ifdef __cplusplus
}
#endif

#endif
