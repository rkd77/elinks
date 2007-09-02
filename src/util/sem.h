#ifndef EL__UTIL_SEM_H
#define EL__UTIL_SEM_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

int sem_create(key_t key, int initval);
int sem_open(key_t key);
void sem_rm(int id);
void sem_close(int id);
void sem_wait(int id);
void sem_signal(int id);
void sem_op(int id, int value);

#endif
