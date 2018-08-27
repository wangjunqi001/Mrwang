#ifndef _SERVER_H__
#define _SERVER_H__ 
#include <string>
#include <unordered_map>
using namespace std;


namespace httpserver{
	 //for more effictive to find the key-value pair
	using Header = unordered_map<string,string>;

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
			int Startup(const unsigned short port);
			int ReadOneRequest(Context* context);
			void NotFound(Response* response);
			void WriteOneResponse(Context* context);
			int HandlerRequest(Context* context);
			int ProcessStaticFile(Context* context);
			int ProcessCGI(Context* context);
			void GetFilePath(const string& file_name,string* output);
		private:
			int ParseFirstline(Request* req,const string& line);
			int ParseUrl(const string& url,string* url_path,string* query_string);
			int ParseHeader(const string& line,Header* header);
	}; 

	struct Context{
		Request request;
		Response response;
		int new_fd;
		HttpServer* server;
	};
}//end of httpserver

#endif







