webserver:
	clang-format -i --style=file http.h event.h reactor.h webserver.c
	gcc -o ./build/webserver webserver.c
run:
	./build/webserver 
