#define VIRTUAL_PIPE_FLAG_WRITE		1
#define VIRTUAL_PIPE_FLAG_NONBLOCK	2

struct virtual_pipe {
	unsigned char content[VIRTUAL_PIPE_SIZE];
	unsigned used;
	unsigned n_rd;
	unsigned n_wr;
};

static struct virtual_pipe *pipe_desc[FD_SETSIZE] = { NULL };
static unsigned pipe_flags[FD_SETSIZE] = { 0 };

static struct virtual_pipe *get_virtual_pipe(int fd)
{
	struct virtual_pipe *desc;
	if (fd >= FD_SETSIZE || !pipe_desc[fd]) return NULL;
	pipe_lock();
	desc = pipe_desc[fd];
	if (!desc) {
		pipe_unlock();
		return NULL;
	}
	return desc;
}

static int vpipe_read(int fd, void *buf, size_t size)
{
	int should_wake;
	struct virtual_pipe *desc;
test_again:
	if (!(desc = get_virtual_pipe(fd)))
		return -2;
	if (pipe_flags[fd] & VIRTUAL_PIPE_FLAG_WRITE) elinks_internal("vpipe_read: reading from write pipe descriptor");
	if (!desc->used) {
		if (!desc->n_wr) {
#ifdef TRACE_PIPES
			fprintf(stderr, "read(%d) -> zero.", fd);
#endif
			pipe_unlock();
			return 0;
		}
		if (pipe_flags[fd] & VIRTUAL_PIPE_FLAG_NONBLOCK) {
#ifdef TRACE_PIPES
			fprintf(stderr, "read(%d) -> wouldblock.", fd);
#endif
			pipe_unlock();
			errno = EAGAIN;
			return -1;
		}
#ifdef TRACE_PIPES
		fprintf(stderr, "read(%d) -> sleep.", fd);
#endif
		pipe_unlock_wait();
		pipe_unlock();
		goto test_again;
	}
	should_wake = desc->used == VIRTUAL_PIPE_SIZE;
	if (size > desc->used)
		size = desc->used;
	memcpy(buf, desc->content, size);
	memmove(desc->content, desc->content + size, desc->used -= size);
#ifdef TRACE_PIPES
	fprintf(stderr, "read(%d) -> %d. (%d)", fd, size, should_wake);
#endif
	pipe_unlock();
	if (should_wake) pipe_wake();
	return size;
}

static int vpipe_write(int fd, const void *buf, size_t size)
{
	int should_wake;
	struct virtual_pipe *desc;
test_again:
	if (!(desc = get_virtual_pipe(fd)))
		return -2;
	if (!(pipe_flags[fd] & VIRTUAL_PIPE_FLAG_WRITE)) elinks_internal("vpipe_write: writing to read pipe descriptor");
	if (!desc->n_rd) {
#ifdef TRACE_PIPES
		fprintf(stderr, "write(%d) -> epipe.", fd);
#endif
		pipe_unlock();
		errno = EPIPE;
		return -1;
	}
	if (desc->used == VIRTUAL_PIPE_SIZE) {
		if (pipe_flags[fd] & VIRTUAL_PIPE_FLAG_NONBLOCK) {
#ifdef TRACE_PIPES
			fprintf(stderr, "write(%d) -> wouldblock.", fd);
#endif
			pipe_unlock();
			errno = EAGAIN;
			return -1;
		}
#ifdef TRACE_PIPES
		fprintf(stderr, "write(%d) -> sleep.", fd);
#endif
		pipe_unlock_wait();
		pipe_unlock();
		goto test_again;
	}
	should_wake = !desc->used;
	if (size > (VIRTUAL_PIPE_SIZE - desc->used))
		size = VIRTUAL_PIPE_SIZE - desc->used;
	memcpy(desc->content + desc->used, buf, size);
	desc->used += size;
#ifdef TRACE_PIPES
	fprintf(stderr, "write(%d) -> %d.", fd, size);
#endif
	pipe_unlock();
	if (should_wake) pipe_wake();
	return size;
}

static int vpipe_close(int fd)
{
	struct virtual_pipe *desc;
	if (!(desc = get_virtual_pipe(fd)))
		return -2;
	if (!(pipe_flags[fd] & VIRTUAL_PIPE_FLAG_WRITE)) {
		if (!desc->n_rd) elinks_internal("vpipe_close: read counter underflow");
		desc->n_rd--;
	} else {
		if (!desc->n_wr) elinks_internal("vpipe_close: write counter underflow");
		desc->n_wr--;
	}
	pipe_desc[fd] = NULL;
	pipe_flags[fd] = 0;
	pipe_unlock();
	if (!desc->n_rd && !desc->n_wr)
		free(desc);
	return close(fd);
}

static int vpipe_create(int fd[2])
{
	int rs;
	struct virtual_pipe *desc;
	EINTRLOOP(fd[0], open("/dev/null", O_RDONLY));
	if (fd[0] == -1)
		goto err0;
	EINTRLOOP(fd[1], open("/dev/null", O_WRONLY));
	if (fd[1] == -1)
		goto err1;
	if (fd[0] >= FD_SETSIZE || fd[1] >= FD_SETSIZE) {
		errno = EMFILE;
		goto err2;
	}
	desc = malloc(sizeof(struct virtual_pipe));
	if (!desc) goto err2;
	desc->used = 0;
	desc->n_rd = 1;
	desc->n_wr = 1;
	pipe_lock();
	if (pipe_desc[fd[0]] || pipe_flags[fd[0]] || pipe_desc[fd[1]] || pipe_flags[fd[0]])
		elinks_internal("c_pipe: pipe handles %d, %d already used: %p, %d, %p, %d", fd[0], fd[1], pipe_desc[fd[0]], pipe_flags[fd[0]], pipe_desc[fd[1]], pipe_flags[fd[0]]);
	pipe_desc[fd[0]] = desc;
	pipe_flags[fd[0]] = 0;
	pipe_desc[fd[1]] = desc;
	pipe_flags[fd[1]] = VIRTUAL_PIPE_FLAG_WRITE;
#ifdef DOS
	pipe_flags[fd[0]] |= VIRTUAL_PIPE_FLAG_NONBLOCK;
	pipe_flags[fd[1]] |= VIRTUAL_PIPE_FLAG_NONBLOCK;
#endif
	pipe_unlock();
	return 0;
err2:
	EINTRLOOP(rs, close(fd[1]));
err1:
	EINTRLOOP(rs, close(fd[0]));
err0:
	return -1;
}

static int vpipe_may_read(int fd)
{
	if (pipe_flags[fd] & VIRTUAL_PIPE_FLAG_WRITE) elinks_internal("vpipe_may_read: selecting write descriptor %d for read", fd);
	return pipe_desc[fd]->used || !pipe_desc[fd]->n_wr;
}

static int vpipe_may_write(int fd)
{
	if (!(pipe_flags[fd] & VIRTUAL_PIPE_FLAG_WRITE)) elinks_internal("vpipe_may_write: selecting read descriptor %d for write", fd);
	return pipe_desc[fd]->used != VIRTUAL_PIPE_SIZE || !pipe_desc[fd]->n_rd;
}

static void clear_inactive(fd_set *fs, int i)
{
	if (!fs) return;
	while (--i >= 0) FD_CLR(i, fs);
}

