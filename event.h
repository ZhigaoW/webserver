#ifndef EVENT_H
#define EVENT_H

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
#define PORT_COUNT 1

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

#endif
