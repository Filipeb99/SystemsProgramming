client:
	gcc $(wildcard *client.c) -o client -lncurses -lpthread
server:
	gcc $(wildcard *server.c) -o server -lncurses -lpthread
all:
	gcc $(wildcard *client.c) -o client -lncurses -lpthread
	gcc $(wildcard *server.c) -o server -lncurses -lpthread
clean :
	ls | grep -v "\." | grep -v makefile | xargs rm
	ls | grep "\.o" | xargs rm
