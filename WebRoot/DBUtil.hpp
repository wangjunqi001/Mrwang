#ifndef __DBUTIL_H__ 
#define __DBUTIL_H__  

#include <mysql.h>
#include "../util.hpp"

using namespace std;

class DBUtil{
public:
	DBUtil();
	~DBUtil();
	int initDB(const string& server_host,const unsigned short port,const string& user,const string& passwd ,const string& db_name);
	MYSQL_RES* ExecuteSQL(const string& sql_str);
	int CreateTable(const string& sql_str);
private:
	MYSQL* connection;
};

DBUtil::DBUtil(){
	if((connection = mysql_init(NULL)) == NULL)
		LOG(ERROR)<<"DBUtil init"<<endl;
}

DBUtil::~DBUtil(){
	if(connection != NULL)
		mysql_close(connection);
}

int DBUtil::initDB(const string& server_host,const unsigned short port,const string& user,const string& passwd ,const string& db_name){	
	connection = mysql_real_connect(connection,server_host.c_str(),user.c_str(),passwd.c_str(),db_name.c_str(),port,NULL,0);
	if(connection == NULL){
		LOG(ERROR)<<"mysql real connect: please check all paramters usage"<<endl;
		return -1;
	}
	return 0;
}

MYSQL_RES* DBUtil::ExecuteSQL(const string& sql_str){
	if(mysql_query(connection,"set names utf8"))
		LOG(ERROR)<<mysql_errno(connection)<<": "<<mysql_error(connection)<<endl;
	if(!mysql_query(connection,sql_str.c_str())){
		LOG(ERROR)<<"mysql_query: invalid sql_str"<<endl;
		return NULL;
	}else 
		return mysql_use_result(connection);
}



#endif






















