#include <string.h>

#define BUFFER_LENGTH 2048

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

int main()
{
        struct ntyevent *ev = (struct ntyevent*)malloc(sizeof(struct ntyevent)); 

        printf("hello\n");

	const char *html =
		"<html><head><title>webserver<title/><head/><body><h1>reactor web server<h1/><body/><html/>";

	sprintf(ev->buffer, "HTTP/1.1 200 OK\r\n\
         Date: Sun, 11 Sep 2022 11:24:51 GMT\r\n\
         Content-Type: text/html; charset=utf-8\r\n\
         Content-Length: %ld\r\n\r\n",
			     strlen(html), html);


        printf("hello\n");
        
        printf(ev->buffer);
        
        return 0;
}
