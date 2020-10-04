#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <sys/signal.h>
#include <printf.h>
#include <curl/curl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "gfserver.h"
#include "cache-student.h"
#include "shm_channel.h"
#include "steque.h"
#include "simplecache.h"
#include <time.h>

#if !defined(CACHE_FAILURE)
#define CACHE_FAILURE (-1)
#endif // CACHE_FAILURE

#define MAX_CACHE_REQUEST_LEN 5041

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		// you should do IPC cleanup here
		exit(signo);
	}
}

unsigned long int cache_delay;

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 7, Range is 1-31415)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-5000000 (microseconds)\n "	\
"  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int get_min(int x, int y, int z) {
        if (x <= y && x <= z) {
                return x;
        } else if (y <= x && y <= z) {
                return y;
        } else {
                return z;
        }
}

int file_size(int f) {
         struct stat finfo;
         memset(&finfo, 0, sizeof finfo);
         fstat(f, &finfo);
         return finfo.st_size;
}


void shared_transfer(int s, segdata *ptr, int fd, int off) {
    
	if (s <= 0) {
     
		return;
   
	}
   
	int fsize = file_size(fd);
   
	int fsize_rem = fsize - off;
   
	if (fsize_rem <= 0) {
     
		return;
   
	}
   
	int psize = getpagesize();
   
	int moff = (off / psize) * psize;
   
	int off_cur = off - moff;
   
	int psize_rem = psize - off_cur;

   
	int load = get_min(fsize_rem , psize_rem, s );
   
	void* data = mmap(0, psize, PROT_READ, MAP_SHARED, fd, moff);
   
	int tsize = ptr->size;
   
	void* fcontent = &ptr->content;
   
	fcontent += tsize;
   
	memmove(fcontent, (data + off_cur), load);
   
	ptr->size = tsize + load;
   
	ptr->size_rem = fsize_rem - load;
   
	munmap(data, psize);
   
	shared_transfer(s-load, ptr, fd, off + load);
}

steque_t *req_queue;
pthread_mutex_t qm;
pthread_cond_t item_exist;

void *cache_worker(void *arg){
        while(1) {
                pthread_mutex_lock (&qm);
                pthread_cond_wait(&item_exist, &qm);
                while (!steque_isempty(req_queue)) {
		
			//printf("In function \nthread id = %ld\n", pthread_self());
                        struct cacheinfo *cinfo;
                        cinfo = (cacheinfo*)steque_pop(req_queue);
                        if (cinfo == NULL) {
                                continue;
                        }
                        void* ptr = (void *)shmat(cinfo->sid, 0, 0);
                        segdata* seg_data = (segdata*)ptr;
                        if (seg_data->filed == 0)
                        {
                                seg_data->filed = simplecache_get(cinfo->path);
                        }
                        if (seg_data->filed < 0) {
                                seg_data->size = -1;
                                sem_post(&(seg_data->slock));
                                free(cinfo);
                                continue;
                        }

                        seg_data->size = 0;
                        shared_transfer(cinfo->msize - sizeof(segdata), seg_data, seg_data->filed, cinfo->stbyte);

                        sem_post(&(seg_data->slock));
                        free(cinfo->path);
                        free(cinfo);
                
                }
			pthread_mutex_unlock(&qm);
        }
}


int main(int argc, char **argv) {
	int nthreads = 7;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "id:c:hlxt:", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;
			case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'l': // experimental
			case 'x': // experimental
				break;
		}
	}

	if (cache_delay > 5000000) {
		fprintf(stderr, "Cache delay must be less than 5000000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>31415) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads\n");
		exit(__LINE__);
	}

	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}

	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}
        simplecache_init(cachedir);

        pthread_mutex_init(&qm, NULL);

        pthread_cond_init (&item_exist, NULL);

        req_queue = malloc (sizeof(steque_t));

        steque_init(req_queue);

        pthread_t threads[nthreads];

        long t;
        for (t=0; t<nthreads; t++) {
                pthread_create(&threads[t], NULL, cache_worker, NULL);
        }

        while (1) {
                cacheinfo* cinfo = cache_recv();
                pthread_mutex_lock (&qm);
                steque_enqueue(req_queue, (void*)cinfo);
                pthread_mutex_unlock(&qm);
                pthread_cond_broadcast(&item_exist);
        }

	return 0;
}
