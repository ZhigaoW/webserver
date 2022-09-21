webserver:
	clang-format -i --style=file http.h event.h reactor.h webserver.c
	gcc -o ./build/webserver webserver.c
run:
	./build/webserver 

io_uring_server:
	gcc io_uring_server_test.cpp -o io_uring_server_test -luring
