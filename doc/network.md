# LINUX NETWORK

## 性能常用工具

### 参考资料

1. [Netflix 高性能架构师 Brendan Gregg's Blog](https://www.brendangregg.com/)
2. [linux command search](https://wangchujiang.com/linux-command/)

---

|        |          |        |               |         |         |         |         |
| ------ | -------- | ------ | ------------- | ------- | ------- | ------- | ------- |
| ipcs   | uptime   | iostat | sar           | mpstat  | pmap    | nmon    | glances |
| ftptop | powertop | mytop  | htop/top/atop | netstat | ethtool | tcpdump | telnet  |
| strace | iptraf   | iftop  |               |         |         |         |         |

## 网络原理代码

## SOCKET

![](./socket.png)

1. [What is the Difference Between read() and recv() , and Between send() and write()?](https://stackoverflow.com/questions/1790750/what-is-the-difference-between-read-and-recv-and-between-send-and-write)
2. [Socket programming - What's the difference between listen() and accept()?](https://stackoverflow.com/questions/34073871/socket-programming-whats-the-difference-between-listen-and-accept)



```c
#include < sys/socket.h >
listenfd = int socket(int domain, int type, int protocol);
```

- domain : 选择网络层协议 可以选择IPV6或者IPV4等
- type : 选择传输层协议 可以选择TCP或UPD等
- protocol : 具体的PROCOTOL 0值为默认
- return : 成功返回一个socket的文件描述符 失败返回-1

```c
#include < sys/socket.h >
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

- bind的作用 : 绑定具体的地址，但是一般把这一步叫命名socket.

```c
#include < sys/socket.h >
int listen(int sockfd, int backlog);
```

- backlog : 全联接的数量

```c
#include 
int accept(int sockfd, struct sockaddr *restrict addr, socklen_t *restrict addrlen);
```



### 客户端较少时C10k 

```c
// 每个线程的线程栈预分配的大小为 8M
// 8M x 128 = 1G 
// 1G的内存大概能有128个客户端连接
void *client_routine(void *arg)
{
        int connfd = *(int *)arg;
        char buff[MAXLINE];

        while (1) {
                int n = recv(connfd, buff, MAXLINE, 0);
                if (n > 0) {
                        buff[n] = '\0';
                        printf("recv msg from client: %s\n", buff);
                } else if (n == 0) {
                        close(connfd);
                }
        }
}

int main(int argc, char **argv)
{
        // 创建一个socket，内核初始化socket相关的数据结构，比如接收消息的队列
  			// socket data structure init in kernel
        listenfd = socket(AF_INET, SOCK_STREAM, 0))

        // 绑定端口号
        // initialization
        ...

        // listenfd (local_ip, local_port, ..., tcp)
        bind(listenfd, (struct sockaddr *)&severaddr, sizeof(severaddr))

        // https://man7.org/linux/man-pages/man2/listen.2.html
        // listenfd listen remote fd
        listen(listenfd, 10)

        while (1) {
          
         				// return connfd (remote_ip, remote_port, local_ip, local_port)
                connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)
                pthread_t threapid;
                pthread_create(&threapid, NULL, client_routine, &connfd);
        }

        close(listenfd);
        return 0;
}
```





## I/O MULTIPLEXING

### Epoll

[Linux and I/O completion ports?](https://stackoverflow.com/questions/2794535/linux-and-i-o-completion-ports)

```c
#include <sys/epoll.h>
// The epoll API can be used either as an edge-triggered or a
// level-triggered interface and scales well to large numbers of
// watched file descriptors.
```

epoll API的核心概念是**内核空间**的数据结构**epoll instance**，
在**用户空间**的视角看来，epoll instance 是两个列表的容器。

- The interest list : 监控列表
- The ready list : I/O ready 列表

**创建epoll instance**

```c
// epoll_create() creates a new epoll instance.
int epoll_create(int size);
int epoll_create1(int flags);
```

```c
/*
* 可以[op]操作epfd[epoll instance]里的fd[io file descripe]
*/
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
```

```c
// epoll_event *event 是用来表明fd[io file descripe]的可读取状态
// union
typedef union epoll_data {
        void        *ptr;
        int          fd;
        uint32_t     u32;
        uint64_t     u64;
} epoll_data_t;
 
struct epoll_event {
        uint32_t     events;      /* Epoll events */
        epoll_data_t data;        /* User data variable */
};
/*
* 
*/ 
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
```

#### EXAMPLE CODE

```c
#define MAX_EVENTS 10
struct epoll_event ev, events[MAX_EVENTS];

int listen_sock, conn_sock, nfds, epollfd;

/* Code to set up listening socket, 'listen_sock',
        (socket(), bind(), listen()) omitted. */

epollfd = epoll_create1(0);
if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
}

/*
* struct epoll_event {
*		 uint32_t events;
*		 epoll_data_t data;
* }
*
*/

ev.events = EPOLLIN;
ev.data.fd = listen_sock;

if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
}

for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
        perror("epoll_wait");
        exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; ++n) {
        if (events[n].data.fd == listen_sock) {
                conn_sock = accept(listen_sock,
                                (struct sockaddr *) &addr, &addrlen);
                if (conn_sock == -1) {
                perror("accept");
                exit(EXIT_FAILURE);
                }
                setnonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                        &ev) == -1) {
                perror("epoll_ctl: conn_sock");
                exit(EXIT_FAILURE);
                }
        } else {
                do_use_fd(events[n].data.fd);
        }
        }
}
```

## 服务器Reactor与Proactor

## HTTP 服务器

1. ntyreactor_run : 服务器不断运行
2. accept_cb : ip限制，负载均衡
3. recv_cb
4. send_cb 



一个web服务器的必要组件

1. http + reactor
2. GET
3. POST
4. cgi



`netstat -anop | grep 8888` 



http发送文件调用sendfile() --> 零拷贝

- 文件DMA-> memory; memory -> mmap





















