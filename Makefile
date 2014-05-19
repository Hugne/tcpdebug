server: server.c
	gcc -lpthread -o server server.c -I.

clean:
	rm -rf server
	rm -rf *.o
