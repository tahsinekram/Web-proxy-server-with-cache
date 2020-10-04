#include "gfserver.h"
#include "cache-student.h"
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <semaphore.h>
#include "shm_channel.h"
#include <time.h>

ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){

	struct cacheinfo req;
	req.sid = dequeue_seg();
	req.path = (char*) path;
	req.stbyte = 0;
	req.msize = get_seg_size();
	
	void* seg_ptr = (void *)shmat(req.sid, 0, 0);
	segdata* seg_data = (segdata*)seg_ptr;
	
	sem_init(&seg_data->slock, 1, 0);

	cache_send(req);
	sem_wait(&seg_data->slock);

	if (seg_data->size <= 0) {
		queue_seg(req.sid);
		sem_destroy(&seg_data->slock);
		return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
	}
	int fsize = seg_data->size + seg_data->size_rem;
	gfs_sendheader(ctx, GF_OK, fsize);

	int sent = 0;

	while(sent < fsize) {
		gfs_send(ctx, &seg_data->content, seg_data->size);
		sent += seg_data->size;
		if (seg_data->size_rem > 0) {
			struct cacheinfo new_req;
			new_req.path = (char*) path;
			new_req.stbyte = sent;
			new_req.msize = get_seg_size();
			new_req.sid = req.sid;
			cache_send(new_req);
			sem_wait(&seg_data->slock);
		}
	}

	sem_destroy(&seg_data->slock);
	queue_seg(req.sid);
	return sent;
}

