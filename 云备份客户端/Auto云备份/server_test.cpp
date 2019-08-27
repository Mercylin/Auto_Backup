#include<iostream>
#include "httplib.h"
void Hello(const httplib::Request &req,httplib::Response &rsp)
{
	rsp.set_content("<html><h1>Hello Mercylin</h1></html>","text.html");
}
int main()
{
	httplib::Server srv;
	srv.set_base_dir("./WWW");
	srv.Get("/", Hello);
	srv.listen("0.0.0.0", 9000);
	system("pause");
	return 0;
}