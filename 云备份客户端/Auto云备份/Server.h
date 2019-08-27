#pragma once
#include "httplib.h"
#include <boost/filesystem.hpp>
#include<iostream>
#include<fstream>
#include<sstream>
#define SERVER_BASE_DIR "WWW"
#define SERVER_ADDR "0.0.0.0"
#define SERVER_PORT 9000
#define SERVER_BACKIP_DIR SERVER_BASE_DIR"/list"

using namespace httplib;
namespace bf = boost::filesystem;
class CloudServer
{
private:
	Server srv;
public:
	CloudServer()
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
		std::cout << "backup file:[" << req.path <<"] range:[" << range <<"] body:["<< req.body<< "] \n";
		std::string real = SERVER_BASE_DIR + req.path;
		std::ofstream file(real, std::ios::binary | std::ios::trunc);//
		if (!file.is_open())
		{
			std::cerr << "open file" << real << "error\n";
			rsp.status = 500;
			return;
		}
		file.seekp(range_start, std::ios::beg);
		file.write(&req.body[0], req.body.size());
		if (!file.good())
		{
			std::cerr << "file write body error\n";
			rsp.status = 500;
			return;
		}
		file.close();
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
		bf::path list(SERVER_BACKIP_DIR);
		bf::directory_iterator item_begin(list);
		bf::directory_iterator item_end;

		std::string body;
		body = "<html><body><ol><hr />";
		for (; item_begin != item_end; ++item_begin)
		{
			if (bf::is_directory(item_begin->status()))  //判断是否是目录
			{
				continue;
			}
			std::string file = item_begin->path().filename().string();
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
		//req.path = "/list/a.txt";  uri
		std::string file = SERVER_BASE_DIR + req.path;  //www/list/a.txt
		if (!bf::exists(file))
		{
			rsp.status = 404;
			return;
		}
		std::ifstream Inputfile(file, std::ios::binary);
		if (!Inputfile.is_open())
		{
			std::cerr << "open file" << file << "error\n";
			rsp.status = 500;
			return;
		}
		std::string body;
		int64_t fsize = bf::file_size(file);
		body.resize(fsize);
		Inputfile.read(&body[0], fsize);
		if (!Inputfile.good())      //good()判断上一步是否正确   bad()判断出错
		{
			std::cerr << "read file" << file << "body error\n";
			rsp.status = 500;
			return;
		}
		rsp.set_content(body, "text/plain");
	}
	static void BackUpFile(const Request &req, Response &rsp) {}
};

