#include <iostream>
#include <cgicc/Cgicc.h>
#include <cgicc/HTMLClasses.h>

using namespace std;
using namespace cgicc;
int main(){
	cout<<"Content-type: html/text"<<endl;
	try{
		  
	}catch(const exception& e){
		cout<<e.what()<<endl;
		return 1;
	}
	return 0;
}
