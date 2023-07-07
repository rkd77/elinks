#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include "protocol/fsp/lock.h"

/* ************ Locking functions ***************** */
#ifndef FSP_NOLOCKING

static char code_str[] =
    "0123456789:_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
make_key_string(FSP_LOCK *lock, unsigned long server_addr, unsigned long server_port)
{
	unsigned long v1, v2;
	char *p;

	strcpy(lock->key_string, FSP_KEY_PREFIX);

	for (p = lock->key_string; *p; p++);

	v1 = server_addr;
	v2 = server_port;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	v1 = v1 | (v2 << (32-3*6));
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p++ = code_str[v1 & 0x3f]; v1 >>= 6;
	*p   = 0;
}
#endif

/********************************************************************/
/******* For those systems that has SysV shared memory + semop ******/
/********************************************************************/
#ifdef FSP_USE_SHAREMEM_AND_SEMOP

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _SEM_SEMUN_UNDEFINED
union semun
{
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

unsigned short
client_get_key(FSP_LOCK *lock)
{
	struct sembuf sem;
	sem.sem_num = 0;
	sem.sem_op = -1;
	sem.sem_flg = SEM_UNDO;

	if (semop(lock->lock_sem,&sem,1) == -1) {
		perror("semop lock");
	}

	return (*lock->share_key);
}

void
client_set_key(FSP_LOCK *lock,unsigned short key)
{
	struct sembuf sem;

	sem.sem_num = 0;
	sem.sem_op  = 1;
	sem.sem_flg = SEM_UNDO;

	*lock->share_key = key;

	if (semop(lock->lock_sem,&sem,1) == -1) {
		perror("semop unlock");
	}
}

int
client_init_key(FSP_LOCK *lock, unsigned long server_addr, unsigned short server_port)
{
	mode_t omask;
	key_t lock_key;
	int fd;
	union semun su;
	struct sembuf sem;

	make_key_string(lock, server_addr, server_port);
	omask = umask(0);
	fd = open(lock->key_string, O_RDWR|O_CREAT, 0666);
	umask(omask);
	close(fd);

	if ((lock_key = ftok(lock->key_string, 238)) == -1) {
		perror("ftok");
		return -1;
	}

	if ((lock->lock_shm = shmget(lock_key, 2 * sizeof(unsigned int), IPC_CREAT|0666)) == -1) {
		perror("shmget");
		return -1;
	}

	if (!(lock->share_key = (unsigned int *)shmat(lock->lock_shm, NULL, 0))) {
		perror("shmat");
		return -1;
	}

	if ((lock->lock_sem = semget(lock_key, 0, 0)) == -1) {
		/* create a new semaphore and init it */
		if ((lock->lock_sem = semget(lock_key, 2, IPC_CREAT|0666)) == -1) {
			perror("semget");
			return -1;
		}
		/* we need to init this semaphore */
		su.val = 1;

		if (semctl(lock->lock_sem, 0, SETVAL,su) == -1) {
			perror("semctl setval");
			return -1;
		}
	}
	/* increase in use counter */
	sem.sem_num = 1;
	sem.sem_op = 1;
	sem.sem_flg = SEM_UNDO;

	if (semop(lock->lock_sem, &sem, 1) == -1) {
		perror("semop incuse");
	}

	return 0;
}

void
client_destroy_key(FSP_LOCK *lock)
{
	int rc;
	struct sembuf sem;

	if (shmdt(lock->share_key) < 0) {
		perror("shmdt");
		return;
	}
	/* check if we are only one process holding lock */
	rc = semctl(lock->lock_sem, 1, GETVAL);

	if (rc == 1) {
		/* safe to destroy */
		if (
		    (semctl(lock->lock_sem,0,IPC_RMID) < 0) ||
		    (shmctl(lock->lock_shm,IPC_RMID,NULL) < 0) ||
		    (unlink(lock->key_string) < 0) ) {
			rc = 0;/* ignore cleanup errors */
		}
	} else if (rc > 1) {
		/* we need to decrease sem. */
		sem.sem_num = 1;
		sem.sem_op = -1;
		sem.sem_flg = SEM_UNDO;

		if (semop(lock->lock_sem,&sem,1) == -1) {
			perror("semop decuse");
		}
	}
}
#endif


/********************************************************************/
/******* For those who do not want to use locking *******************/
/********************************************************************/
#ifdef FSP_NOLOCKING

unsigned short
client_get_key(FSP_LOCK *lock)
{
	return lock->share_key;
}

void
client_set_key(FSP_LOCK *lock,unsigned short key)
{
	lock->share_key = key;
}

int
client_init_key(FSP_LOCK *lock, unsigned long server_addr, unsigned short server_port)
{
	return 0;
}

void
client_destroy_key(FSP_LOCK *lock)
{
	return;
}
#endif

/********************************************************************/
/******* For those systems that has lockf function call *************/
/********************************************************************/
#ifdef FSP_USE_LOCKF

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

unsigned short
client_get_key(FSP_LOCK *lock)
{
	unsigned int okey;

	if (lockf(lock->lock_fd, F_LOCK, sizeof(okey)) < 0) {
		perror("lockf");
	}

	if (read(lock->lock_fd, &okey, sizeof(okey)) < 0) {
		perror("readlk");
	}

	if (lseek(lock->lock_fd, 0L, 0) < 0) {
		perror("seek");
	}

	return(okey);
}

void
client_set_key(FSP_LOCK *lock,unsigned short nkey)
{
	unsigned int key;
	key = nkey;

	if (write(lock->lock_fd, &key, sizeof(key)) < 0) {
		perror("write");
	}

	if (lseek(lock->lock_fd, 0L, 0) < 0) {
		perror("seek");
	}

	if (lockf(lock->lock_fd, F_ULOCK, sizeof(key)) < 0) {
		perror("unlockf");
	}
}

int
client_init_key(FSP_LOCK *lock, unsigned long server_addr, unsigned short server_port)
{
	mode_t omask;
	make_key_string(lock,server_addr, server_port);
	omask = umask(0);
	lock->lock_fd = open(lock->key_string, O_RDWR | O_CREAT, 0666);
	(void)umask(omask);

	if (lock->lock_fd < 0) {
		return -1;
	} else {
		return 0;
	}
}

void
client_destroy_key(FSP_LOCK *lock)
{
	(void)close(lock->lock_fd);
}
#endif
