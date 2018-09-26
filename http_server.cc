#include "http_server.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "util.hpp"
#include <sys/wait.h>
#include <sys/stat.h>
#include "timer.h"
#include "epoll.h"

using sockaddr_in = struct sockaddr_in;
using SA = struct sockaddr;

pthread_mutex_t server_construct = PTHREAD_MUTEX_INITIALIZER;

namespace httpserver{

	HttpServer* HttpServer::Instance = NULL;

	void* thread_entry(void* arg){
		Context* context = reinterpret_cast<Context*>(arg);
		HttpServer* server = HttpServer::GetInstance();
		int	ret = server->ReadOneRequest(context);
			if( ret < 0 ){
				LOG(ERROR)<<"ReadOneRequest"<<endl;
				server->NotFound(&context->response);
				server->WriteOneResponse(context);
				goto END;
			}
			if(ret < 0 ){
				LOG(ERROR)<<"HandlerRequest"<<endl;
				server->NotFound(&context->response);
				server->WriteOneResponse(context);
				goto END;
			}
END:server->WriteOneResponse(context);
		Epoll* http_ep = server->GetEpoll();
		http_ep->Delfd(context->new_fd,EPOLLIN|EPOLLET,context);
		close(context->new_fd);
		delete context;
		return NULL;
	}

	HttpServer* HttpServer::GetInstance(){
		if(Instance == NULL){
			pthread_mutex_lock(&server_construct);
			if(Instance == NULL)
				Instance = new HttpServer;
			pthread_mutex_unlock(&server_construct);
		}
		return HttpServer::Instance;
	}

	void HttpServer::DestoryInstance(){
		delete Instance;
		Instance = NULL;
	}

	HttpServer::HttpServer(){
		int listen_sock = socket(PF_INET,SOCK_STREAM,0);
		if(listen_sock < 0)
			error_die("socket");
		int optval = 1;
		if(setsockopt(listen_sock , SOL_SOCKET , SO_REUSEADDR , (const void*)&optval , sizeof(int)) == -1)
			error_die("setsockopt");
		sockaddr_in sock;
		socklen_t len = sizeof(sock);
		sock.sin_family = AF_INET;
		sock.sin_addr.s_addr = htonl(INADDR_ANY);
		sock.sin_port = htons(HTTP_SERVER_DEFAULT_TCP_PORT);
		if( bind(listen_sock,(SA*)&sock,len) < 0 ){
			close(listen_sock);
			error_die("bind");
		}
		if( listen(listen_sock , 5) < 0 ){
			close(listen_sock);
			error_die("listen");
		}
		http_tp = new thread_pool(HTTP_SERVER_DEFAULT_MIN_THR,HTTP_SERVER_DEFAULT_MAX_THR,HTTP_SERVER_DEFAULT_JOB_BUFFER_SIZE);
		if(FileUtil::SetfdNonblock(listen_sock) == -1){
			close(listen_sock);
			error_die("Set sock nonblock failed");
		}
		http_ep = new Epoll(0 , listen_sock , http_tp);
#ifdef debug
		LOG(DEBUG)<<"Server start ok! Listen Socket = "<< listen_sock << endl;
#endif
	}

	void HttpServer::AcceptRequest(){
		sockaddr_in client_addr;
		socklen_t len = sizeof(client_addr);
		int new_fd = accept(http_ep->GetListenSock() , (sockaddr*)&client_addr , &len);
		if(new_fd == -1){
			LOG(ERROR) << "accept failed" << endl;
			return;
		}
		Context* context = new Context();
		if(FileUtil::SetfdNonblock(new_fd) < 0)
			LOG(ERROR) << "SetfdNonblockd failed" << endl;
#ifdef debug
		LOG(DEBUG) << "new_fd = "<< new_fd << endl;
#endif 
		http_ep->Addfd(new_fd , EPOLLET|EPOLLIN , context);
#ifdef debug
		LOG(DEBUG) << "add accept socket = " << new_fd << endl;
#endif 
	}

	void HttpServer::ServerLoop(){
		while(1){
			int events_num = http_ep->Wait();
			if(events_num <= 0)
				continue;
#ifdef debug
	LOG(DEBUG) << "ready event num = " << events_num << endl;
#endif 
			http_ep->HandlerEvents(events_num);
		}
		close(http_ep->GetListenSock());
	}

	HttpServer::~HttpServer(){
		delete http_tp;
		delete http_ep;
	}

	Epoll* HttpServer::GetEpoll(){
		return http_ep; 
	}

	int HttpServer::ReadOneRequest(Context* context){
		Request* requ = &context->request;
		string line;
		FileUtil::ReadLine(context->new_fd,&line);
		int ret = ParseFirstline(requ,line);
#ifdef debug
		LOG(DEBUG)<<"[method & url]-- "<<requ->method.c_str()<<" "<<requ->url.c_str()<<endl;
#endif 
		if(ret < 0){
			LOG(ERROR)<<"ParseFirstline error";
			return -1;
		}
		UrlDecode(requ->url);
   	ret = ParseUrl(requ->url,&requ->url_path,&requ->query_string);
#ifdef debug
		LOG(DEBUG)<<"[url_path & query_string]-- "<<requ->url_path.c_str()<<" "<<requ->query_string.c_str()<<endl;
#endif
		while(1){
			line.clear();
			FileUtil::ReadLine(context->new_fd,&line);
			if(line == "")
				break;
			if( ParseHeader(line,&requ->header) < 0)
				error_die("ParseHeader error");
		}
		auto it = requ->header.find("Content-Lenth");
		if(it == requ->header.end() && requ->method == "POST"){
			LOG(ERROR)<<"POST method has no Content-Lenth!"<<endl;
			return -1;
		}
		if(requ->method == "GET")
			return 0;
		if(it != requ->header.end())
			FileUtil::ReadN(context->new_fd,atoi(it->second.c_str()),&requ->body);
#ifdef debug
		LOG(DEBUG)<<"[body]-- "<<requ->body.c_str()<<endl;
#endif 
		return 0;
	}

	int HttpServer::ParseFirstline(Request* req,const string& line){
		vector<string> out;
		StringUtil::Split(line," ",&out);
		if(out.size() != 3){
			LOG(ERROR)<<"ParseFirstline miss member"<<endl;
			return -1;
		}
		if(line.find("HTTP") == string::npos){
			LOG(ERROR)<<"ParseFirstline version miss 'HTTP'"<<endl;
			return -1;
		}
		req->method = out[0];
		req->url = out[1];
		return 0;
	}

	int HttpServer::ParseUrl(const string& url,string* url_path,string* query_string){
		size_t pos = url.find("?");
		if(pos == string::npos){
			*url_path = url.substr(0);
			query_string = NULL;
			return 0;
		}
		*url_path = url.substr(0,pos);
		*query_string = url.substr(pos+1);
		return 0;
	}

	void HttpServer::UrlDecode(const string& Url){ // 对URL解码 如C%2B%2B -> c++ 这只能简单解决中文URL的问题，不能支持所有Unicode
		string DstUrl;
		DstUrl.reserve(Url.size());
		size_t n = Url.size();
		for(size_t i = 0 ; i < n ; ++i){
			if(Url[i] == '%'){
				if(i + 1 < n && isxdigit(Url[i + 1]) && i + 2 < n && isxdigit(Url[i + 2])){
					int a = xtoi(Url[i + 1]);
					int b = xtoi(Url[i + 2]);
					DstUrl.push_back( (a << 4) | b );
				}else
				DstUrl.push_back('%');
			}
			else
				DstUrl.push_back(Url[i]);
		}
	}

	//HOST: 0.0.0.0
	int HttpServer::ParseHeader(const string& line,Header* header){
		size_t pos = line.find(":");
		if(pos == string::npos){
			LOG(ERROR)<<"ParseHeader: Header miss ':'"<<endl;
			return -1;
		}
		if(pos + 2 >= line.size()){
			LOG(ERROR)<<"ParseHeader: Header miss value"<<endl;
			return -1;
		}
		(*header)[line.substr(0,pos)] = line.substr(pos + 2);
#ifdef debug
		LOG(DEBUG)<<"[Header]-- "<<line.substr(0,pos)<<": "<<line.substr(pos + 2)<<endl;
#endif
		return 0;
	}
	
	void HttpServer::NotFound(Response* resp){
		resp->state = 404;
		resp->desc = "Not Found";
		resp->body = "<h1>您访问的页面被喵星人吃掉了！</h1>";
		stringstream ss;
		ss<<resp->body.size();
		resp->header["Content-Lenth"] = ss.str();
	}

	int HttpServer::HandlerRequest(Context* context){ 
		Request* req = &context->request;
		Response* resp = &context->response;
		resp->state = 200;
		resp->desc = "OK";
		struct stat st;
		string file_path;
		GetFilePath(req->url_path,&file_path);
		if(stat(file_path.c_str(),&st) == -1){
			NotFound(resp);
			return 0;
		}else if(st.st_mode & S_IXUSR ||
				 		 st.st_mode & S_IXGRP ||
						 st.st_mode & S_IXOTH){
			ProcessCGI(context);
			return 0;
		}
		if(req->method == "GET" && req->query_string == "")
			ProcessStaticFile(context);
		else if(req->method=="POST" || (req->method == "GET" && req->query_string!=""))
			ProcessCGI(context);
		else{
			LOG(ERROR)<<"Unsupported method :"<<req->method<<endl;
			return -1;
		}
		return 0;
	}

	void HttpServer::WriteOneResponse(Context* context){
		Response& resp = context->response;
		stringstream ss;
		ss << "HTTP/1.1" <<' '<< resp.state <<' '<< resp.desc <<'\n';
		if(resp.cgi_resp.length()==0){
			for(auto& it : resp.header)
				ss << it.first <<": " <<it.second << '\n';
			ss << '\n';
			ss << resp.body;
		}
		else
			ss<< resp.cgi_resp;
		const string& str = ss.str();
		write(context->new_fd,str.c_str(),str.size());
	}

	int HttpServer::ProcessStaticFile(Context* context){
		Request* req = &context->request;
		Response* resp = &context->response;
		string file_path;
		GetFilePath(req->url_path,&file_path);
		if(FileUtil::ReadAll(file_path,&resp->body) < 0){
			LOG(ERROR)<<"ReadAll"<<endl;
			return -1;
		}
		return 0;
	}

	int HttpServer::ProcessCGI(Context* context){
		const Request& req = context->request;
		Response* resp = &context->response;
		int fd1[2],fd2[2];
		pipe(fd1);
		pipe(fd2);
		int father_read = fd1[0];
		int child_write = fd1[1];
		int father_write = fd2[1];
		int child_read = fd2[0];
		string method = "METHOD="+req.method;
		putenv(const_cast<char*>(method.c_str()));
		if(req.method == "GET"){
			string query_string = "QUERY_STRING="+req.query_string;
			putenv(const_cast<char*>(method.c_str()));
		}else if(req.method == "POST"){
			auto pos = req.header.find("Context-Lenth");
			string content_lenth = "CONTENT_LENTH=" + pos->second;
			putenv(const_cast<char*>(content_lenth.c_str()));
		}
		pid_t id = fork();
		if(id < 0){
			LOG(ERROR)<<"fork"<<endl;
			return -1;
		}
		else if(id > 0){
			close(child_write);
			close(child_read);

			if(req.method == "POST")
				write(father_read,req.body.c_str(),req.body.size());
			FileUtil::ReadAll(father_read,&resp->cgi_resp);
			close(father_read);
			close(father_write);
			wait(NULL);
		}else{
			close(father_read);
			close(father_write);

			dup2(child_read,0);
			dup2(child_write,1);
			string file_path;
			GetFilePath(req.url_path,&file_path);
			execl(file_path.c_str(),file_path.c_str(),NULL);
		}
		return 0;
	}

	void HttpServer::GetFilePath(const string& url_path,string* file_path){
		*file_path = "./WebRoot" + url_path;
		if(FileUtil::Isdir(*file_path)){
			if(file_path->back() != '/')
				*file_path += "/";
			*file_path += "index.html";
		}
		return;
	}
}//end of httpserver

int main(){
	httpserver::HttpServer* server = httpserver::HttpServer::GetInstance();
	server->ServerLoop();
	pthread_mutex_destroy(&server_construct);
	httpserver::HttpServer::DestoryInstance();
	return 0;
}














