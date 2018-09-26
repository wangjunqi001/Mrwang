.PHONY:all
all:http_server

http_server:http_server.cc epoll.cc ThreadPool.cc 
	g++ -o $@ $^ -Ddebug -std=c++11 -lpthread -lboost_filesystem -lboost_system 
ini_test:ini_test.cc ini_parser.cc
	g++ -o $@ $^ -Ddebug -std=c++11 -lboost_filesystem -lboost_system
.PHONY:clean
clean:
	rm http_server ini_parser
