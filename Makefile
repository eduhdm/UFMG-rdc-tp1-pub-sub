all:
	g++ -Wall -c common.cpp
	g++ -Wall client.cpp common.o -o client
	g++ -Wall server-mt.cpp common.o -lpthread -o server-mt

clean:
	rm common.o client server-mt
