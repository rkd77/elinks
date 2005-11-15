#ifndef EL__MAIN_SELECT_H
#define EL__MAIN_SELECT_H

typedef void (*select_handler_T)(void *);

/* Start the select loop after calling the passed @init() function. */
void select_loop(void (*init)(void));

/* Get information about the number of descriptors being checked by the select
 * loop. */
int get_file_handles_count(void);

/* Schedule work to be done when appropriate in the future. */
int register_bottom_half_do(select_handler_T work_handler, void *data);

/* Wrapper to remove a lot of casts from users of bottom halves. */
#define register_bottom_half(fn, data) \
	register_bottom_half_do((select_handler_T) (fn), (void *) (data))

/* Check and run scheduled work. */
void check_bottom_halves(void);

enum select_handler_type {
	SELECT_HANDLER_READ,
	SELECT_HANDLER_WRITE,
	SELECT_HANDLER_ERROR,
	SELECT_HANDLER_DATA,
};

/* Get a registered select handler. */
select_handler_T get_handler(int fd, enum select_handler_type type);

/* Set handlers and callback @data for the @fd descriptor. */
void set_handlers(int fd,
		  select_handler_T read_handler,
		  select_handler_T write_handler,
		  select_handler_T error_handler,
		  void *data);

/* Clear handlers associated with @fd. */
#define clear_handlers(fd) \
	set_handlers(fd, NULL, NULL, NULL, NULL)

/* Checks which can be used for querying the read/write state of the @fd
 * descriptor without blocking. The interlink code are the only users. */
int can_read(int fd);
int can_write(int fd);

#endif
