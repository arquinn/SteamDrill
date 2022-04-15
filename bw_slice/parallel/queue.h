#include "streamserver.h"

#ifndef _QUEUE_H
#define _QUEUE_H

#define BUCKET_TERM_VAL 0xfffffffe // Sentinel for queue transmission
#define TERM_VAL        0xffffffff // Sentinel for queue transmission
#define QSTOP(val) (((val)&0xfffffffe)==0xfffffffe)
#define QEND(val) ((val)==TERM_VAL)


char bucket_emptied[TAINTBUCKETS], bucket_filled[TAINTBUCKETS];
bool finished;
uint32_t next_read_index, next_write_index;

static void inline
bucket_write_init ()
{
    memset (bucket_filled, 0, sizeof(bucket_filled));
    finished = false;
    next_write_index = 0;
}

static void inline
bucket_read_init ()
{
    memset (bucket_emptied, 0, sizeof(bucket_emptied));
    next_read_index = 0;
}

static void inline 
bucket_init ()
{
    bucket_write_init();
    bucket_read_init();
}

static void inline 
bucket_push (uint32_t val, struct taintq_hdr* qh, uint32_t*& qb, uint32_t& bucket_cnt, uint32_t& bucket_stop)
{

    //why do pre and post checking here? 

    if (bucket_cnt == bucket_stop) {
	// Get next bucket 
	pthread_mutex_lock(&(qh->lock));
	while ((next_write_index+1)%TAINTBUCKETS == qh->read_index) {
#ifdef STATS
	    struct timeval tv1, tv2;	       
	    gettimeofday(&tv1, NULL);	       
#endif
	    pthread_cond_wait(&(qh->full), &(qh->lock));
#ifdef STATS
	    gettimeofday(&tv2, NULL);		
	    send_idle += ms_diff (tv2,tv1);
#endif
	}
	bucket_cnt = next_write_index * TAINTBUCKETENTRIES;
	bucket_stop = bucket_cnt + TAINTBUCKETENTRIES;
	next_write_index = (next_write_index+1)%TAINTBUCKETS;
	pthread_mutex_unlock(&(qh->lock));
    } 
    qb[bucket_cnt++] = val;
    
    if (bucket_cnt == bucket_stop) {

	// This bucket is done
	pthread_mutex_lock(&(qh->lock));
	uint32_t bucket_index = (bucket_cnt-1)/TAINTBUCKETENTRIES;
	if (bucket_index == qh->write_index) {
	    // Mark this and any following emptied buckets as writable
	    qh->write_index = (qh->write_index+1)%TAINTBUCKETS;
	    while (bucket_filled[qh->write_index]) {
		bucket_filled[qh->write_index] = 0;
		qh->write_index = (qh->write_index+1)%TAINTBUCKETS;
	    }
	    pthread_cond_broadcast(&(qh->empty));
	} else {
	    bucket_filled[bucket_index] = 1;
	}
	pthread_mutex_unlock(&(qh->lock));
    }
}

// Pushes the bucket even if it is half-full - append sentinel to show this
static void inline bucket_term (struct taintq_hdr* qh, uint32_t*& qb, uint32_t& bucket_cnt, uint32_t& bucket_stop)
{

    if (bucket_cnt == bucket_stop) return;  // Have not grabbed a bucket yet - so nothing to do

    if (bucket_cnt && QEND(qb[bucket_cnt-1])) { 
	qb[bucket_stop-1] = TERM_VAL; // Mark last bucket for network processing
    }
    else {
	qb[bucket_stop-1] = 0; // Mark as *NOT* last bucket for network processing
    }   
    
    if (bucket_stop - 1 != bucket_cnt)
	qb[bucket_cnt++] = BUCKET_TERM_VAL;  // Mark bucket as done


    bucket_stop = bucket_cnt;  // Force new bucket
    pthread_mutex_lock(&(qh->lock));
    uint32_t bucket_index = (bucket_cnt-1)/TAINTBUCKETENTRIES;
    if (bucket_index == qh->write_index) {
	// Mark this and any following emptied buckets as writable
	qh->write_index = (qh->write_index+1)%TAINTBUCKETS;
	while (bucket_filled[qh->write_index]) {
	    bucket_filled[qh->write_index] = 0;
	    qh->write_index = (qh->write_index+1)%TAINTBUCKETS;
	}
	pthread_cond_broadcast(&(qh->empty));
    } else {
	bucket_filled[bucket_index] = 1;
    }
    pthread_mutex_unlock(&(qh->lock));
}

static void inline 
bucket_pull (uint32_t& val, struct taintq_hdr* qh, uint32_t*& qb,  uint32_t& bucket_cnt, uint32_t& bucket_stop)
{
    do {
	if (bucket_cnt == bucket_stop) {
	    // Get next bucket 
	    pthread_mutex_lock(&(qh->lock));
	    while (qh->write_index == next_read_index && !finished) {
#ifdef STATS
		struct timeval tv1, tv2;	       
		gettimeofday(&tv1, NULL);	       
#endif
		pthread_cond_wait(&(qh->empty), &(qh->lock));
#ifdef STATS
		gettimeofday(&tv2, NULL);		
		recv_idle += ms_diff (tv2,tv1);
#endif
	    }
	    if (finished) {
		pthread_mutex_unlock(&(qh->lock));
		val = TERM_VAL;
		return;
	    }
	    bucket_cnt = next_read_index * TAINTBUCKETENTRIES;
	    bucket_stop = bucket_cnt + TAINTBUCKETENTRIES;
	    next_read_index = (next_read_index+1)%TAINTBUCKETS;
	    pthread_mutex_unlock(&(qh->lock));
	} 
	
	val = qb[bucket_cnt++];

	if (bucket_cnt == bucket_stop || QSTOP(val)) {
	    if (QEND(val)) {
		// No more data to come - let other threads know 
		pthread_mutex_lock(&(qh->lock));
		finished = true;
		pthread_cond_broadcast(&(qh->empty));
		pthread_mutex_unlock(&(qh->lock));
		return;
	    }

	    // This bucket is done
	    pthread_mutex_lock(&(qh->lock));
	    uint32_t bucket_index = (bucket_cnt-1)/TAINTBUCKETENTRIES;
	    if (bucket_index == qh->read_index) {
		// Mark this and any following emptied buckets as writable
		qh->read_index = (qh->read_index+1)%TAINTBUCKETS;
		while (bucket_emptied[qh->read_index]) {
		    bucket_emptied[qh->read_index] = 0;
		    qh->read_index = (qh->read_index+1)%TAINTBUCKETS;
		}
		pthread_cond_broadcast(&(qh->full));
	    } else {
		bucket_emptied[bucket_index] = 1;
	    }
	    pthread_mutex_unlock(&(qh->lock));
	    if (val == BUCKET_TERM_VAL) {
		bucket_stop = bucket_cnt; // Force new bucket
	    }
	}
    } while (val == BUCKET_TERM_VAL);
}

/*
static u_long bucket_wait_term (struct taintq_hdr* qh, uint32_t*& qb)
{
    pthread_mutex_lock(&(qh->lock));

    while (qh->write_index == qh->read_index || !QEND(qb[qh->write_index*TAINTBUCKETENTRIES-1])) {	
	pthread_cond_wait(&(qh->empty), &(qh->lock));	
    }
    pthread_mutex_unlock(&(qh->lock));
    u_long ndx = (qh->write_index-1)*TAINTBUCKETENTRIES;   
    while (QSTOP(qb[ndx])) {
	if (ndx == 0) return 0;
	ndx -= TAINTBUCKETENTRIES;
    }
    do {
	ndx++;
    } while (!QSTOP(qb[ndx]));
    return ndx;
}

static void bucket_complete_write (struct taintq_hdr* qh, uint32_t*& qb, uint32_t& bucket_cnt)
{
    pthread_mutex_lock(&(qh->lock));
    qb[bucket_cnt] = TERM_VAL;
    qh->write_index = bucket_cnt / TAINTBUCKETENTRIES + 1;
    qb[qh->write_index*TAINTBUCKETENTRIES-1] = TERM_VAL; // For network processing
    pthread_cond_broadcast(&(qh->empty));
    pthread_mutex_unlock(&(qh->lock));
}
*/
#define PUT_QVALUE(val,q,qb,bc,bs) bucket_push (val,q,qb,bc,bs);
#define GET_QVALUE(val,q,qb,bc,bs) bucket_pull (val,q,qb,bc,bs);

#define DOWN_QSEM(qh) sem_wait(&(qh)->epoch_sem);
#define UP_QSEM(qh) sem_post(&(qh)->epoch_sem);

#endif
