#ifndef _SERVER_H__
#define _SERVER_H__ 
#include <string>
#include <unordered_map>
using namespace std;

#define HTTP_SERVER_DEFAULT_MIN_THR 5
#define HTTP_SERVER_DEFAULT_MAX_THR 200
#define HTTP_SERVER_DEFAULT_JOB_BUFFER_SIZE 100
#define HTTP_SERVER_DEFAULT_TCP_PORT 8080
#define HTTP_SERVER_DEFAULT_WEB_ROOT "./Webroot/"
#define HTTP_SERVER_DEFAULT_WEB_FILE "index.ht" 

struct timer;
class thread_pool;
class Epoll;

namespace httpserver{
	 //for more effictive to find the key-value pair
	using Header = unordered_map<string,string>;
  void* thread_entry(void* arg);
	/*
	 * HTTP request object 
	 */ 
	struct Request{
		string method;
		string url;
		string url_path;
		string query_string;
		Header header;
		string body;
	};

	/*  
	 *  HTTP response from server 
	 */ 
	struct Response{
		int state;
		string desc;
		Header header;
		string body;
		string cgi_resp;
	};

	/*
	 *	server's main class to read request and write response
	 */ 
	struct Context;
	class HttpServer{
		public:
			static HttpServer* GetInstance();
			static void DestoryInstance();
			int ReadOneRequest(Context* context);
			void NotFound(Response* response);
			void WriteOneResponse(Context* context);
			int HandlerRequest(Context* context);
			int ProcessStaticFile(Context* context);
			int ProcessCGI(Context* context);
			void GetFilePath(const string& file_name,string* output);
			void AcceptRequest();
			void ServerLoop();
			Epoll* GetEpoll();
		private:
			HttpServer();
			~HttpServer();
			static HttpServer* Instance;
			int ParseFirstline(Request* req,const string& line);
			int ParseUrl(const string& url,string* url_path,string* query_string);
			int ParseHeader(const string& line,Header* header);
			void UrlDecode(const string& Url);
		private:
			thread_pool* http_tp;
			Epoll* http_ep;
	};

	struct Context{
		Request request;
		Response response;
		int new_fd;
		timer* time_counter;
	};
}//end of httpserver

#endif







