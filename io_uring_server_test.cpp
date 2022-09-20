/*
        io_uring 就像一个送货机制，把数据放到一个地方，内核自己运输，运输好了通知用户 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

// #include <liburing/io_uring.h>

#include "liburing.h"

#define PORT 6666
#define ENTRY_LENGTH 10

enum {
        ACCEPT,
        RECV,
        SEND,
};

struct ioinfo {
        int fd;
        int type;
};

int main() 
{
        int listenfd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd == -1) {
                return -1;
        }
        
        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(PORT);

        if (-1 == bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
                return -2;
        }

        listen(listenfd, 10);

        
        // init io_uring
        struct io_uring_params params;
        memset(&params, 0, sizeof(params));

        struct io_uring ring;
        memset(&ring, 0, sizeof(ring));
        
        io_uring_queue_init_params(ENTRY_LENGTH, &ring, &params);



        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        struct sockaddr_in client;
        socklen_t len = sizeof(client);

        struct ioinfo info = {
                .fd = listenfd,
                .type = ACCEPT
        };

        memcpy(&(sqe->user_data), &info, sizeof(info));

        io_uring_prep_accept(sqe, listenfd, (struct sockaddr*)&client, &len, 0);

        while(1) {
                // 提交到内核里
                io_uring_submit(&ring);
                struct io_uring_cqe *cqe;
                int ret = io_uring_wait_cqe(&ring, &cqe);

                struct io_uring_cqe *cqes[10];
                int count = io_uring_peek_batch_cqe(&ring, cqes, 10);
                int i = 0;
                for (i = 0; i < count; i++) {
                        cqe = cqes[i];
                        struct ioinfo ii;
                        memcpy(&ii, &(cqe->user_data), sizeof(ii));
                }
                

        }
        




        return 0;
}
