#ifndef EL__TERMINAL_HARDIO_H
#define EL__TERMINAL_HARDIO_H

ssize_t hard_write(int fd, unsigned char *data, size_t datalen);
ssize_t hard_read(int fd, unsigned char *data, size_t datalen);

#endif
