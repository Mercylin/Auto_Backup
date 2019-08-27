#pragma once
#include "httplib.h"
#include"compress.hpp"
#include <boost/filesystem.hpp>
#include<unistd.h>
#include<fcntl.h>
#include<iostream>
#include<fstream>
#include<sstream>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define SERVER_BASE_DIR "www"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKIP_DIR SERVER_BASE_DIR"/list"

using namespace httplib;
namespace bf = boost::filesystem;

CompressStore cstor;

class CloudServer
{
private:
	//Server srv;
	SSLServer srv;
public:
	CloudServer(const char *cert,const char *key):srv(cert,key)
	{
		bf::path base_path(SERVER_BASE_DIR);
		//判断文件是否存在
		if (!bf::exists(base_path))
		{
			bf::create_directory(base_path);
		}
		bf::path list_path(SERVER_BACKIP_DIR);
		if (!bf::exists(list_path))
		{
			bf::create_directory(list_path);
		}
	}
	bool Start()
	{
		srv.set_base_dir(SERVER_BASE_DIR);
		srv.Get("/(list(/){0,1}){0,1}", GetFileList);
		srv.Get("/list/(.*)", GetFileData);  //匹配任意文件名  .*匹配任一字符任意次
		srv.Put("/list/(.*)", PutFileData);
		srv.listen(SERVER_ADDR, SERVER_PORT);
		return true;
	}
private:
	static void PutFileData(const Request &req, Response &rsp)
	{


		if (~req.has_header("Range"))
		{
			rsp.status = 400;
			return;
		}
		std::string range = req.get_header_value("Range");
		int64_t range_start;
		if (req.has_header("Range"))//判断是否有range字段
		{
			//int64_t range_start;
			if (RangeParse(range, range_start) == false)
			{
				rsp.status = 400;
				return;
			}
		}
	
		std::cout << "backup file:[" << req.path << "] range:[" << range << "]\n";
		std::string real = SERVER_BASE_DIR + req.path;

		cstor.SetFileData(real, req.body ,range_start);


		//int fd = open(real.c_str(), O_CREAT | O_WRONLY, 0644);
		//if (fd < 0)
		//{
		//	std::cerr << "open file" << real << "error\n";
		//	rsp.status = 500;
		//	return;
		//}
		//lseek(fd, range_start, SEEK_SET);
		////file.seekp(range_start, std::ios::beg);
		////file.write(&req.body[0], req.body.size());
		//int ret = write(fd, &req.body[0]);
		//if (ret != req.body.size())
		//{
		//	std::cerr << "file write body error\n";
		//	rsp.status = 500;
		//	return;
		//}
		//close(fd);
		return;
	}
	static bool RangeParse(std::string &range, int64_t &start)        //解析range
	{
		//Range: bytes = start - end;
		size_t pos1 = range.find("=");
		size_t pos2 = range.find("-");
		if (pos1 == std::string::npos || pos2 == std::string::npos)  //如果没有找到pos
		{
			std::cerr << "range:[" << range << "] format error\n";
			return false;
		}
		std::stringstream rs;
		rs << range.substr(pos1 + 1, pos2 - pos1 - 1);
		rs >> start;
		return true;
	}
	static void GetFileList(const Request &req, Response &rsp)  //获取文件列表
	{
		/*bf::path list(SERVER_BACKIP_DIR);
		bf::directory_iterator item_begin(list);
		bf::directory_iterator item_end;*/
		std::vector<std::string> list;
		cstor.GetFileList(list);
		std::string body;
		body = "<html><body><ol><hr />";
		for(auto i:list)                       // (; item_begin != item_end; ++item_begin)
		{
			//if (bf::is_directory(item_begin->status()))  //判断是否是目录
			//{
			//	continue;
			//}
			bf::path path(i);

			std::string file = path.filename().string();
			std::string uri = "/list/" + file;
			body += "<h3><li>";
			body += "<a href='";
			body += uri;
			body += "'>";
			body += file;
			body += "</a>";
			body += "</li></h3>";
			//<h3><li><a href='/list/filename'>filename</a></li></h3> 
		}
		body += "<hr /></ol></body></html>";
		rsp.set_content(&body[0], "text/html");
		return;
	}
	static void GetFileData(const Request &req, Response &rsp) 
	{
		std::string real = SERVER_BASE_DIR + req.path;
		std::string body;
		cstor.GetFileData(real, body);
		rsp.set_content(body, "text/plain");
		//req.path = "/list/a.txt";  uri
		//std::cerr << "into GetFileData \n";
		//std::string file = SERVER_BASE_DIR + req.path;  //www/list/a.txt
		//if (!bf::exists(file))
		//{
		//	std::cerr << "file" << file << "is not exists\n";
		//	rsp.status = 404;
		//	return;
		//}
		//std::ifstream Inputfile(file, std::ios::binary);
		//if (!Inputfile.is_open())
		//{
		//	std::cerr << "open file" << file << "error\n";
		//	rsp.status = 500;
		//	return;
		//}
		//std::string body;
		//int64_t fsize = bf::file_size(file);
		//body.resize(fsize);
		//Inputfile.read(&body[0], fsize);
		//if (!Inputfile.good())      //good()判断上一步是否正确   bad()判断出错
		//{
		//	std::cerr << "read file" << file << "body error\n";
		//	rsp.status = 500;
		//	return;
		//}
		
	}
};
void thr_start()
{
	cstor.LowHeatFileStore();
}
int main()
{
	std::thread thr(thr_start);
	thr.detach();
	CloudServer srv("./cert.pem","./key.pem");
	srv.Start();
	return 0;
}