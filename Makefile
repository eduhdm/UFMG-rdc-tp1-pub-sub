all:
	g++ -Wall -c lib/common.cpp
	g++ -Wall -c lib/server-utils.cpp
	g++ -Wall client.cpp common.o  -lpthread -o cliente
	g++ -Wall server.cpp common.o server-utils.o -lpthread -o servidor

clean:
	rm common.o server-utils.o cliente servidor
