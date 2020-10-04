# Web-proxy-server-with-cache

This porject implements a proxy server that handles the translation of calls from a client to http requests that obtains file contents. The proxy server also implements a cache to handle faster response to files requested for previously. The design diagram image provides a overview of the design. The key concepts involved in this design is interprocess communication, multi-threading and synchronization, semaphore and mutex usage and shared memory operations.

The code flow of is defined as follows
1. Proxy initializes a pool of shared memory segments and stores their id in a message queue.
2. Upon receiving a request the proxy pops a shared id from the message queue
3. It initializes the semaphore for that specific memory segment
4. Sends the request containing file name, segment size and byte to start mapping from to the cache using a another message queue and waits for the initialized semaphore to be unlocked
5. As cache receives a request it allocates a worker thread to parse it an contain the relevant information sent.
6. Using the path name received from the cache it checks if the file is available in the cache and if not it removes the lock on the semaphore and sets the size of the share to an invalid value.
7. Next a function invoked by the cache worker recursively copies the contents of the file to the shared memory as much as the size of the shared memory segment.
8. When shared memory segment is 0 or less the semaphore the proxy is waiting on in the handle_with_cache function is waiting on is released
9. Next the proxy sends all available data in the shared memory via gfs send.
10. The total length of file and he total sent is tracked to make sure whole file is sent.
11. Until the condition that bytes sent is 0 the proxy continues making a request to newer file portions of the file changing the start byte every time.

The interface for handling the following was implemented via the shm_channel.c file:

1. creating segments
2. popping and pushing into message queues
3. generating and receiving requests to and from cache
4. cleanup of shared memory segments and message queues created
