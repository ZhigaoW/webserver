// reactor解决的问题:
//      1. 对于一个读/写了一半的数据，可以把它存起来，可以记录偏移量
//      2. 处理readbuffer/writebuffer，方便业务分层
/*      如何理解reactor的分层：
 *              3. 处理数据             --> call_back
 *              2. read/write数据      --> recv/send
 *              1. 检测I/O事件          -->  epoll
 */

#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BACKLOG 10
#define PORT 6666
#define MAX_EVENT 1024
#define MAX_BUF_LENGTH 2048
#define MAX_EPOLL_EVENT 1024


/* I/O event */
struct event {
	int	      fd;
        int           event;
	int	      status;   // 这个值是干啥的
	unsigned char buffer[MAX_BUF_LENGTH];
	int	      bufpos;
	int (*callback)(int fd, int event, void *arg);
        long last_active; // 最后一次链接，处理一些长期不用的连接
};

/* event storage block */
struct eventblock {
        struct eventblock *next;
        struct event *item
};

/* reactor */
struct reactor {
        int epfd;
        struct eventblock *head;
        struct eventblock **last;
};

int init_reactor(struct reactor *r) 
{
        if (r == NULL) {
                printf("reactor is null\n");
                return -1;
        }

        // r->epfd 
        r->epfd = epoll_create(1);
        r->head = (struct eventblock *)malloc(sizeof(struct eventblock));
	if (r->head == NULL) {
		close(r->epfd);
		return -2;
	}
	memset(r->head, 0, sizeof(struct eventblock));

	r->head->item = malloc(MAX_EPOLL_EVENT * sizeof(struct event));
	if (r->head->item == NULL) {
		free(r->head);
		close(r->epfd);
		return -2;
	}
	memset(r->head->item, 0, MAX_EPOLL_EVENT * sizeof(struct event));
	r->head->next = NULL;
        return 0;
}


struct reactor *R = NULL;
struct reactor *singleton_reactor(void) 
{
        if (R == NULL) {
                // malloc失败返回的是null吗
                R = malloc(sizeof(struct reactor));
                if (R == NULL) {
                        return NULL;
                }
                memset(R, 0, sizeof(struct reactor));
                // 这个判断来嘎哈的
                if (init_reactor(R) < 0) {
                        return NULL;
                }
        }
        return R;
}


int init_socket(int port)
{
	/* INIT */
	int		   listenfd;
	struct sockaddr_in severaddr;

	/* CREATE AN SOCKET */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		printf("create sever socket error: %s(errno: %d)\n",
		       strerror(errno), errno);
		return -1;
	}

	/* INIT */
	memset(&severaddr, 0, sizeof(severaddr));
	severaddr.sin_family	  = AF_INET;
	severaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	severaddr.sin_port	  = htons(port);

	/* BIND SOCKET */
	if (bind(listenfd, (struct sockaddr *)&severaddr, sizeof(severaddr)) ==
	    -1) {
		printf("bind sever socket error: %s(errno: %d)\n",
		       strerror(errno), errno);
		return -1;
	}

	/* LISTEN SOCKET */
	if (listen(listenfd, BACKLOG) == -1) {
		printf("listen sever socket error: %s(errno: %d)\n",
		       strerror(errno), errno);
		return -1;
	}
	return listenfd;
}


int main()
{
	int listenfd;
	listenfd = init_socket(PORT);




	int epfd = epoll_create(1);

	struct epoll_event ev, event[MAX_EVENT];
	ev.data.fd = listenfd;
	ev.events  = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
	//
}
