#ifndef EL__PROTOCOL_FSP_LOCK_H
#define EL__PROTOCOL_FSP_LOCK_H 1

#define FSP_NOLOCKING 1

#ifndef FSP_NOLOCKING
/* define locking prefix if needed */
# ifndef FSP_KEY_PREFIX
#  define FSP_KEY_PREFIX "/tmp/.FSPL"
# endif
#endif

#ifdef FSP_USE_SHAREMEM_AND_SEMOP

typedef struct FSP_LOCK {
		unsigned int *share_key;
		int   lock_shm;
		int   lock_sem;
		char key_string[sizeof(FSP_KEY_PREFIX)+32];
} FSP_LOCK;

#elif defined(FSP_NOLOCKING)

typedef struct FSP_LOCK {
               unsigned short share_key;
} FSP_LOCK;

#elif defined(FSP_USE_LOCKF)

typedef struct FSP_LOCK {  
	       int lock_fd;
	       char key_string[sizeof(FSP_KEY_PREFIX)+32];
} FSP_LOCK;

#else
#error "No locking type specified"
#endif

/* prototypes */

unsigned short client_get_key (FSP_LOCK *lock);
void client_set_key (FSP_LOCK *lock,unsigned short key);
int client_init_key (FSP_LOCK *lock,
                            unsigned long server_addr,
			    unsigned short server_port);
void client_destroy_key(FSP_LOCK *lock);
#endif
