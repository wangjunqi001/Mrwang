#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <sys/epoll.h>
#include "ThreadPool.h"

#define MAXEVENTS 1024

class Epoll{
public:
	//自动加入listen_fd方便后续处理连接请求
  Epoll(int flag , int listen_fd , thread_pool* http_tp);
	~Epoll();
	int Addfd(int fd , int events , void* arg);
	int Delfd(int fd , int events , void* arg);
	int Modfd(int fd , int events , void* arg);
	int Wait(); // return count of ready fd
	void HandlerEvents(int events_num);
	int GetListenSock();
	int GetEpollfd();
private:
	int _Common(int fd , int events , int op , void* arg);
private:
	int epoll_fd;
	int listen_sock;
	epoll_event* events;
	thread_pool* phttp_tp;
};
#endif




