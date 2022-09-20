#include "reactor.h"

/**********************************************
 * 1. accept_cb : ip限制，负载均衡
 * 2. recv_cb   :
 * 3. send_cb   :
**********************************************/

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
