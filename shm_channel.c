#define     _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>

#include "shm_channel.h"


static int SEGMENT_SIZE = -1;

void seg_create(int n, int s) 
{
  
	SEGMENT_SIZE = s;
  
	for (int i = 0; i < n; i++) {
    
		key_t k;
    
		int   sid;
    
		k = ftok(".", i);
    
		if((sid = shmget(k, s, IPC_CREAT|IPC_EXCL|0666)) == -1) {
      
			if((sid = shmget(k, s, 0)) == -1) {
        
				exit(1);
      
			}
    
		}
  
	}
}

int get_seg_size() 
{
  
	return SEGMENT_SIZE;
}

int get_seg_id(int i, int s) {
  
	key_t k = ftok(".", i);
  
	return shmget(k, s, 0);
}

void remove_seg(int n, int s) 
{
  
	for (int i = 0; i < n; i++) {
    
		key_t k;
    
		int   sid;
    
		k = ftok(".", i);
    
		sid = shmget(k, s, 0);
    
		shmctl(sid, IPC_RMID, 0);
  
	}
}

int push_to_queue(long type, char *text) {
  
	key_t key;
  
	int mq_id;
  
	struct mymsgbuf qbuf;

  
	key = ftok(".", 'm');

  
	if((mq_id = msgget(key, IPC_CREAT|0660)) == -1) {
    
		free(text);
    
		return -1;
  
	}

  
	qbuf.mtype = type;
  
	strcpy(qbuf.mtext, text);

  
	if((msgsnd(mq_id, &qbuf, strlen(qbuf.mtext)+1, 0)) ==-1) {
    
		free(text);
    
		return -1;
  
	}
  
	free(text);
  
	return 1;
}
void queue_seg(int id) {

	void* ptr = (void *)shmat(id, 0, 0);

        segdata* seg_data = (segdata*)ptr;

	seg_data->filed = 0;
			

	char *idString;
  
	asprintf(&idString, "%d", id);
  
	push_to_queue(SMSG_KEY, idString);
}

int dequeue_seg() {
  
	char* msg = pop_from_queue(SMSG_KEY);
  
	int id = atoi(msg);
  
	free(msg);
  
	return id;
}

char* pop_from_queue(long type) 
{
  
	key_t key;
  
	int   mq_id;
  
	struct mymsgbuf qbuf;

  
	key = ftok(".", 'm');

  
	if((mq_id = msgget(key, IPC_CREAT|0660)) == -1) {
    
		return NULL;
  
	}

  
	qbuf.mtype = type;
  
	msgrcv(mq_id, &qbuf, MAX, type, 0);
  
	char *returnString = malloc(strlen(qbuf.mtext) + 1);
  
	strcpy(returnString, qbuf.mtext);
  
	return returnString;
}

void remove_msg_queue() 
{
  
	key_t key;
  
	int   mq_id;

  
	key = ftok(".", 'm');
  
	mq_id = msgget(key, IPC_CREAT|0660);
  
	msgctl(mq_id, IPC_RMID, 0);
}

void cache_send(cacheinfo request) 
{
  
	char *reqString;
  
	asprintf(&reqString, "%d%s%d%s%d%s%s",
                    
			request.sid,
                    
			DELIM,
                    
			request.stbyte,
			
			DELIM,
                    
			request.msize,
                    
			DELIM,
                    
			request.path);
  
	push_to_queue(CMSG_KEY, reqString);
}

cacheinfo* cache_recv() {
  
	char* req = pop_from_queue(CMSG_KEY);
	char *path;
  
	struct cacheinfo *request = malloc(sizeof(cacheinfo));

  
	char* mem;
  
	char* start;
  
	char* size;
  
	char* p;


  
	mem = strtok(req, DELIM);
  
	start = strtok(NULL, DELIM);
  
	size = strtok(NULL, DELIM);
  
	p = strtok(NULL, DELIM);

  
	if (mem == NULL || start == NULL || p == NULL || size == NULL) {
    
		free(req);
    
		free(request);
    
		return NULL;
  
	}

  
	int st = atoi(start);
  
	int msize = atoi(size);
  
	if (msize < 0) {
    
		free(request);
    
		free(req);
    
		return NULL;
  
	}
  
	if (st < 0) {
    
		free(request);
    
		free(req);
    
		return NULL;
  
	}
  
	int smem = atoi(mem);
  
	request->stbyte = st;
  
	request->sid = smem;
  
	request->msize = msize;
  
  
  
	asprintf(&path, "%s", p);
  
	request->path = path;
 
	free(req);
  
	return request;
}

