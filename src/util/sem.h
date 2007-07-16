#ifndef EL__UTIL_SEM_H
#define EL__UTIL_SEM_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

int mysem_create(key_t key, int initval);
int mysem_open(key_t key);
void mysem_rm(int id);
void mysem_close(int id);
void mysem_wait(int id);
void mysem_signal(int id);
void mysem_op(int id, int value);

#endif
