#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>

#define MAX   256
#define DELIM "****"
#define CMSG_KEY 2
#define SMSG_KEY 1

#define SEMAPHORE_ID 64

typedef struct cacheinfo {
  int sid;
  int msize;
  //int idx;
  int stbyte;
  char* path;
} cacheinfo;

typedef struct mymsgbuf {
  long mtype;
  char mtext[MAX];
} mymsgbuf;

typedef struct segdata {
  sem_t slock;
  int size;
  int filed;
  int size_rem;
  void* content;
} segdata;

int push_to_queue(long type, char *text);
char* pop_from_queue(long type);
void remove_msg_queue();

void seg_create(int n, int s);
void remove_seg(int n, int s);
int get_seg_id(int i, int s);
int get_seg_size();

void queue_seg(int id);
int dequeue_seg();

void cache_send(cacheinfo request);
cacheinfo* cache_recv();
