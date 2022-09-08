// 0. data struct
//      1. event 对应于一个io事件的描述
//      2. eventblock 存储一堆io事件的block
//      3. reactor 监听epfd和事件eventblock的集合

// 1. socket_init()
//      初始化socket，并监听端口port，返回listenfd

// 2. reactor_init()
//      初始化和reactor相关的数据结构，这一步里有epoll_create

// 3. reactor_addlitener()
//      将listenfd加入reactor队列，这一步主要调用epoll_ctl
//              1. event_set
//              2. event_add
//              3. event_del
//              4. accept_cb

// 4. reactor_run
//      这一步调用epoll_wait，如果发现有I/O事件，调用callback
//              1. send_cb
//              2. recv_cb

// 5. reactor_destory
//      释放reactor的资源

// 6. socket_destory
//      释放socket的资源




/***************************************************************
SOME QUESTION
1. 存储event的数据结构为啥用 链表 + 数组，event事件为啥不会被删除
***************************************************************/

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

#define BUFFER_LENGTH 4096
#define MAX_EPOLL_EVENTS 1024
#define SERVER_PORT 8888
#define PORT_COUNT 100

typedef int NCALLBACK(int, int, void *);

// 一个connectfd一个ntyevent吗
// 还是一个事件一个ntyevent
struct ntyevent {
	int   fd;
	int   events;
	void *arg;
	int (*callback)(int fd, int events, void *arg);

	int status; // 后面用于判断fd有没有被加入epoll内核的数据结构
	// 如果没有读完就到了写的时候，数据会不会乱掉
	char buffer[BUFFER_LENGTH];
	int  length;
	long last_active; // 判断一段时间不发数据fd
};

/*
 用来链表来存储 ntyevent
 */
struct eventblock {
	struct eventblock *next;
	struct ntyevent *  events;
};

/* 
用来存储epoll创建的数据和 eventblock
*/
struct ntyreactor {
	int		   epfd;
	int		   blkcnt;
	struct eventblock *evblk; //fd --> 100w
};

int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);

// 通过socketfd, 找到reactor中ntyevent的位置
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd);

// 用fd，callback，*arg来设置ntyevent
void nty_event_set(struct ntyevent *ev, int fd, NCALLBACK callback, void *arg)
{
	ev->fd		= fd;
	ev->callback	= callback;
	ev->events	= 0;
	ev->arg		= arg;
	ev->last_active = time(NULL);

	return;
}

// ev第一次的时候是ADD，其他时候都是读状态和写状态的转换
// 因为第二次已经是被加入的状态
int nty_event_add(int epfd, int events, struct ntyevent *ev)
{
	// 初始化了一个新数据
	struct epoll_event ep_ev = { 0, { 0 } };
	ep_ev.data.ptr		 = ev;
	ep_ev.events = ev->events = events;

	int op;
	if (ev->status == 1) {
		op = EPOLL_CTL_MOD;
	} else {
		op	   = EPOLL_CTL_ADD;
		ev->status = 1;
	}

	if (epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0) {
		printf("event add failed [fd=%d], events[%d]\n", ev->fd,
		       events);
		return -1;
	}

	return 0;
}

// 如果被加入epfd，删除，如果没有返回异常
int nty_event_del(int epfd, struct ntyevent *ev)
{
	struct epoll_event ep_ev = { 0, { 0 } };

	if (ev->status != 1) {
		return -1;
	}

	ep_ev.data.ptr = ev;
	ev->status     = 0;
	epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_ev);

	return 0;
}

/*
recive的回调函数:传入的参数是fd，event，*arg 其实是reactor
        1. 将void *arg转化为reactor
        2. 通过reactor和fd找到ntyevent
        3. 接收数据
        4. 从reactor的epfd中删除事件ev
        5. 接收和处理数据
        6. 设置读事件，进行读处理
*/
int recv_cb(int fd, int events, void *arg)
{
	struct ntyreactor *reactor = (struct ntyreactor *)arg;
	struct ntyevent *  ev	   = ntyreactor_idx(reactor, fd);

	// !水平触发，一个recv函数就会读完吗，溢出咋办
	int len = recv(fd, ev->buffer, BUFFER_LENGTH, 0);
	nty_event_del(reactor->epfd, ev);

	if (len > 0) {
		ev->length	= len;
		ev->buffer[len] = '\0';

		printf("C[%d]:%s\n", fd, ev->buffer);

		nty_event_set(ev, fd, send_cb, reactor);
		nty_event_add(reactor->epfd, EPOLLOUT, ev);

	} else if (len == 0) {
		close(ev->fd);
		//printf("[fd=%d] pos[%ld], closed\n", fd, ev-reactor->events);

	} else {
		close(ev->fd);
		printf("recv[fd=%d] error[%d]:%s\n", fd, errno,
		       strerror(errno));
	}

	return len;
}

// 和读操作相反
// !但是没有考虑没有写完咋办，一定可以写完吗
int send_cb(int fd, int events, void *arg)
{
	struct ntyreactor *reactor = (struct ntyreactor *)arg;
	struct ntyevent *  ev	   = ntyreactor_idx(reactor, fd);

	int len = send(fd, ev->buffer, ev->length, 0);
	if (len > 0) {
		printf("send[fd=%d], [%d]%s\n", fd, len, ev->buffer);

		nty_event_del(reactor->epfd, ev);
		nty_event_set(ev, fd, recv_cb, reactor);
		nty_event_add(reactor->epfd, EPOLLIN, ev);

	} else {
		close(ev->fd);

		nty_event_del(reactor->epfd, ev);
		printf("send[fd=%d] error %s\n", fd, strerror(errno));
	}

	return len;
}

// 1. 处理accept
// 2. 然后是等待接收数据
int accept_cb(int fd, int events, void *arg)
{
	// arg reactor
	struct ntyreactor *reactor = (struct ntyreactor *)arg;
	if (reactor == NULL)
		return -1;

	struct sockaddr_in client_addr;
	socklen_t	   len = sizeof(client_addr);

	int clientfd;

	if ((clientfd = accept(fd, (struct sockaddr *)&client_addr, &len)) ==
	    -1) {
		if (errno != EAGAIN && errno != EINTR) {
		}
		printf("accept: %s\n", strerror(errno));
		return -1;
	}

	int flag = 0;
	if ((flag = fcntl(clientfd, F_SETFL, O_NONBLOCK)) < 0) {
		printf("%s: fcntl nonblocking failed, %d\n", __func__,
		       MAX_EPOLL_EVENTS);
		return -1;
	}

	struct ntyevent *event = ntyreactor_idx(reactor, clientfd);

	nty_event_set(event, clientfd, recv_cb, reactor);
	nty_event_add(reactor->epfd, EPOLLIN, event);

	printf("new connect [%s:%d], pos[%d]\n",
	       inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
	       clientfd);

	return 0;
}

// listen port
int init_sock(short port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(fd, F_SETFL, O_NONBLOCK);

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family	    = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port	    = htons(port);

	bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if (listen(fd, 20) < 0) {
		printf("listen failed : %s\n", strerror(errno));
	}

	return fd;
}

int ntyreactor_alloc(struct ntyreactor *reactor)
{
	if (reactor == NULL)
		return -1;
	if (reactor->evblk == NULL)
		return -1;

	struct eventblock *blk = reactor->evblk;
	while (blk->next != NULL) {
		blk = blk->next;
	}

	struct ntyevent *evs = (struct ntyevent *)malloc(
		(MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
	if (evs == NULL) {
		printf("ntyreactor_alloc ntyevents failed\n");
		return -2;
	}
	memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

	struct eventblock *block =
		(struct eventblock *)malloc(sizeof(struct eventblock));
	if (block == NULL) {
		printf("ntyreactor_alloc eventblock failed\n");
		return -2;
	}
	memset(block, 0, sizeof(struct eventblock));

	block->events = evs;
	block->next   = NULL;

	blk->next = block;
	reactor->blkcnt++; //

	return 0;
}

// index
struct ntyevent *ntyreactor_idx(struct ntyreactor *reactor, int sockfd)
{
	int blkidx = sockfd / MAX_EPOLL_EVENTS;

	//
	while (blkidx >= reactor->blkcnt) {
		ntyreactor_alloc(reactor);
	}

	int		   i   = 0;
	struct eventblock *blk = reactor->evblk;
	while (i++ < blkidx && blk != NULL) {
		blk = blk->next;
	}

	return &blk->events[sockfd % MAX_EPOLL_EVENTS];
}

int ntyreactor_init(struct ntyreactor *reactor)
{
	if (reactor == NULL)
		return -1;
	memset(reactor, 0, sizeof(struct ntyreactor));

	reactor->epfd = epoll_create(1);
	if (reactor->epfd <= 0) {
		printf("create epfd in %s err %s\n", __func__, strerror(errno));
		return -2;
	}

	struct ntyevent *evs = (struct ntyevent *)malloc(
		(MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));
	if (evs == NULL) {
		printf("ntyreactor_alloc ntyevents failed\n");
		return -2;
	}
	memset(evs, 0, (MAX_EPOLL_EVENTS) * sizeof(struct ntyevent));

	struct eventblock *block =
		(struct eventblock *)malloc(sizeof(struct eventblock));
	if (block == NULL) {
		printf("ntyreactor_alloc eventblock failed\n");
		return -2;
	}
	memset(block, 0, sizeof(struct eventblock));

	block->events = evs;
	block->next   = NULL;

	reactor->evblk	= block;
	reactor->blkcnt = 1;

	return 0;
}

int ntyreactor_destory(struct ntyreactor *reactor)
{
	close(reactor->epfd);
	//free(reactor->events);

	struct eventblock *blk	    = reactor->evblk;
	struct eventblock *blk_next = NULL;

	while (blk != NULL) {
		blk_next = blk->next;

		free(blk->events);
		free(blk);

		blk = blk_next;
	}

	return 0;
}

// 用reactor->epfd 监听 socketfd
// 1. 注册了acceptor callback
// 2. 加入等待队列
// 3. 如果有事件发生，就会回调accepter
int ntyreactor_addlistener(struct ntyreactor *reactor, int sockfd,
			   NCALLBACK *acceptor)
{
	if (reactor == NULL)
		return -1;
	if (reactor->evblk == NULL)
		return -1;

	//reactor->evblk->events[sockfd];
	struct ntyevent *event = ntyreactor_idx(reactor, sockfd);

	// set event
	nty_event_set(event, sockfd, acceptor, reactor);
	nty_event_add(reactor->epfd, EPOLLIN, event);

	return 0;
}

// 1. 申请了MAX_EPOLL_EVENTS数量的events，这个值应该是一次能拿来的epoll event的值
// ! 2. struct ntyevent *ev = (struct ntyevent *)events[i].data.ptr;
//      1. 可以通过event[i].data.event 来判断事件
//      2. 通过回调函数来处理数据

int ntyreactor_run(struct ntyreactor *reactor)
{
	if (reactor == NULL)
		return -1;
	if (reactor->epfd < 0)
		return -1;
	if (reactor->evblk == NULL)
		return -1;

	struct epoll_event events[MAX_EPOLL_EVENTS + 1];

	int checkpos = 0, i;

	while (1) {
		/*
		long now = time(NULL);
		for (i = 0;i < 100;i ++, checkpos ++) {
			if (checkpos == MAX_EPOLL_EVENTS) {
				checkpos = 0;
			}

			if (reactor->events[checkpos].status != 1) {
				continue;
			}

			long duration = now - reactor->events[checkpos].last_active;

			if (duration >= 60) {
				close(reactor->events[checkpos].fd);
				printf("[fd=%d] timeout\n", reactor->events[checkpos].fd);
				nty_event_del(reactor->epfd, &reactor->events[checkpos]);
			}
		}
                */

		int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_EVENTS,
					1000);
		if (nready < 0) {
			printf("epoll_wait error, exit\n");
			continue;
		}

		for (i = 0; i < nready; i++) {
			struct ntyevent *ev =
				(struct ntyevent *)events[i].data.ptr;

			if ((events[i].events & EPOLLIN) &&
			    (ev->events & EPOLLIN)) {
				ev->callback(ev->fd, events[i].events, ev->arg);
			}
			if ((events[i].events & EPOLLOUT) &&
			    (ev->events & EPOLLOUT)) {
				ev->callback(ev->fd, events[i].events, ev->arg);
			}
		}
	}
}

// 3, 6w, 1, 100 ==
// <remoteip, remoteport, localip, localport>
int main(int argc, char *argv[])
{
	unsigned short port = SERVER_PORT; // listen 8888
	if (argc == 2) {
		port = atoi(argv[1]);
	}

	// 初始化reactor
	struct ntyreactor *reactor =
		(struct ntyreactor *)malloc(sizeof(struct ntyreactor));
	ntyreactor_init(reactor);

	int i			= 0;
	int sockfds[PORT_COUNT] = { 0 };
	for (i = 0; i < PORT_COUNT; i++) {
		sockfds[i] = init_sock(port + i);
		ntyreactor_addlistener(reactor, sockfds[i], accept_cb);
	}

	ntyreactor_run(reactor);

	ntyreactor_destory(reactor);

	for (i = 0; i < PORT_COUNT; i++) {
		close(sockfds[i]);
	}

	free(reactor);

	return 0;
}
