#pragma once
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
using namespace std;
#define BUFF_SZ 1024
enum debug_level{ 
	INFO,
	DEBUG,
	WARNING,
	ERROR 
};

#define LOG(level) log(level,__FILE__,__LINE__)

inline ostream& log(int level, const char* file,const int line){
	string s;
	if(level == INFO) s = "--LOG:";
	else if(level == DEBUG) s = "--DEBUG:";
	else if(level == WARNING) s = "--WARNING";
	else if(level == ERROR) s = "--ERROR";
	cout<<s<<"["<<file<<"->"<<line<<"]:"; 
	return cout;
}

class StringUtil{
public:
	static void Split(const string& input,const string& split_char,vector<string>* output){
		boost::split(*output,input,boost::is_any_of(split_char),boost::token_compress_on);
	}
};

class FileUtil{
public:
	static int ReadLine(int fd, string* line){
		line->clear();
		while(1){
			char c;
			int re_sz = recv(fd,&c,1,0);
			if(re_sz <= 0)
				return -1;
			else if(c == '\n')
				break;
			else if(c == '\r'){
				recv(fd,&c,1,MSG_PEEK);
				if(c == '\n')
					recv(fd,&c,1,0);
				break;
			}
			else
				line->push_back(c);
		}
		return 0;
	}

	static void ReadN(int fd, size_t n, string* out){
		out->clear();
		out->resize(n);
		recv(fd,const_cast<char*>(out->c_str()),n,0);
	}

	static bool Isdir(const string& file_path){
		return boost::filesystem::is_directory(file_path);
	}

	static int ReadAll(const string& file_path ,string* output){
		ifstream ifs(file_path);
		if(!ifs.is_open()){
			LOG(ERROR)<<"Open file :"<<file_path<<endl;
			return -1;
		}
		
		ifs.seekg(0,ifs.end);
		size_t size = ifs.tellg();
		ifs.seekg(0,ifs.beg);

		output->resize(size);
		ifs.read(const_cast<char*>(output->c_str()),size);

		ifs.close();
		return 0;
	}

	static int ReadAll(int fd, string* output){
		char buf[BUFF_SZ] = {0};
		while(1){
			int ret = read(fd,buf,BUFF_SZ);
			if(ret < 0){
				LOG(ERROR)<<"ReadAll"<<endl;
				return -1;
			}else if(ret == 0)
				break;
			output->append(buf);
		}
		return 0;
	}

};

inline void error_die(const string& info){
	LOG(ERROR)<<info<<endl;
	exit(1);
}























