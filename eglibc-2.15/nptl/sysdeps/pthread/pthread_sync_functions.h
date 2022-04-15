#ifndef _PTHREAD_SYNC_H
#define _PTHREAD_SYNC_H 	1


/* Logging functions for external code */
extern int pthread_log__sync_add_and_fetch(int* val, int x);
extern int pthread_log__sync_bool_compare_and_swap(int* val, int x, int y);
extern int pthread_log__sync_fetch_and_add(int* val, int x);
extern int pthread_log__sync_fetch_and_sub(int* val, int x);
extern int pthread_log__sync_lock_test_and_set(int* val, int x);
extern int pthread_log__sync_sub_and_fetch(int* val, int x);
extern int pthread_log__sync_val_compare_and_swap(int* val, int x, int y);
extern unsigned int pthread_log__sync_add_and_fetch_uint(unsigned int* val, unsigned int x);
unsigned int pthread_log__sync_bool_compare_and_swap_uint(unsigned int* val, unsigned int x, unsigned int y);
extern unsigned int pthread_log__sync_sub_and_fetch_uint(unsigned int* val, unsigned int x);
#ifdef uint64_t
extern uint64_t pthread_log__sync_add_and_fetch_uint64(uint64_t* val, uint64_t x);
extern uint64_t pthread_log__sync_sub_and_fetch_uint64(uint64_t* val, uint64_t x);
#endif
