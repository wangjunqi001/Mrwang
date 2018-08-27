.PHONY:all
all:http_server

http_server:http_server.cc
	g++ -o $@ $^ -Ddebug -std=c++11 -lpthread -lboost_filesystem -lboost_system 
.PHONY:clean
clean:
	rm http_server
