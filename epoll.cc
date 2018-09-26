#include "epoll.h"
#include "http_server.h"

typedef struct epoll_event epoll_event;

Epoll::Epoll(int flag , int listen_fd , thread_pool* thread_pool):
	listen_sock(listen_fd),phttp_tp(thread_pool)
{
	epoll_fd = epoll_create1(flag);
	if(epoll_fd < -1){
		LOG(ERROR) << "epoll create failed!" <<endl;
		exit(0);
	}
	epoll_event event;
	httpserver::Context* context = new httpserver::Context();
	event.data.ptr = context;
	context->new_fd = listen_fd;
	event.events = EPOLLIN|EPOLLET;
  int ret = epoll_ctl(epoll_fd , EPOLL_CTL_ADD , listen_fd , &event);
	if(ret < 0){
		LOG(ERROR) << "epoll add listen_fd failed!" << endl;
		exit(0);
	}
	events = new epoll_event[MAXEVENTS];
}

Epoll::~Epoll(){
	close(epoll_fd);
	delete[] events;
}

int Epoll::_Common(int fd , int events , int op , void* arg){
	epoll_event event;
	event.data.ptr = arg;
	reinterpret_cast<httpserver::Context*>(arg)->new_fd = fd;
#ifdef debug
	LOG(DEBUG) << "new_fd = " << fd << endl;
#endif 
	event.events = events;
#ifdef debug
	LOG(DEBUG) << "epoll_ctl start!" << endl;
#endif 
	int ret = epoll_ctl(epoll_fd , op , fd , &event);
	if(ret < 0)
		LOG(ERROR) << "epoll ctl failed" << endl;
	return ret;
}

int Epoll::Addfd(int fd , int events , void* arg){
	return _Common(fd,events,EPOLL_CTL_ADD,arg);
}

int Epoll::Delfd(int fd , int events , void* arg){
	return _Common(fd,events,EPOLL_CTL_DEL,arg);
}

int Epoll::Modfd(int fd , int events , void* arg){
	return _Common(fd,events,EPOLL_CTL_MOD,arg);
}

int Epoll::Wait(){
	return epoll_wait(epoll_fd,events,MAXEVENTS,-1);
}

int Epoll::GetEpollfd(){
	return epoll_fd;
}

int Epoll::GetListenSock(){
	return listen_sock;
}

void Epoll::HandlerEvents(int events_num){
	httpserver::HttpServer* server = httpserver::HttpServer::GetInstance();
	for(int i = 0 ; i < events_num ; ++i){
	  httpserver::Context* context = reinterpret_cast<httpserver::Context*>(events[i].data.ptr);
		int fd = context->new_fd;
#ifdef debug
	LOG(DEBUG) << "context-> fd = " << fd  << endl;
#endif 
		if(fd == listen_sock)
			server->AcceptRequest();
		else{
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)
					|| (events[i].events & EPOLLHUP) ){
				close(fd);
				continue;
			}
			phttp_tp->Add(httpserver::thread_entry , events[i].data.ptr);
		}
	}
}



















