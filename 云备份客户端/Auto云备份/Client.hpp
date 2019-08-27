#pragma once
#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<vector>
#include<unordered_map>
#include<thread>
#include<boost/filesystem.hpp>
#include<boost/algorithm/string.hpp>  //����һ���ַ����и��
#include "httplib.h"

#define CLIENT_BACKUP_DIR "backup"
#define CLIENT_BACKUP_INFO_FILE "back.list"
#define RANGE_MAX_SIZE (10<<20)   //10M   ������10M�ٷֿ飩
#define SERVER_IP "192.168.43.13"
#define SERVER_PORT (9000)
#define BACKUP_URI "/list/"

namespace bf = boost::filesystem;
class ThrBackUp
{
private:
	std::string _file;
	int64_t _range_start;
	int64_t _range_len;
public:
	bool _res;
public:
	ThrBackUp(const std::string &file, int64_t start, int64_t len)
		:_res(true),
		_file(file),
		_range_start(start),
		_range_len(len){}
	void Start()
	{
		//��ȡ�ļ���range�ֿ�����
		std::ifstream path(_file, std::ios::binary);
		if (!path.is_open())
		{
			std::cerr << "range backup file" << _file << "failed\n";
			_res = false;
			return;
		}
		//��ת��range����ʼλ��
		path.seekg(_range_start, std::ios::beg);
		std::string body;
		body.resize(_range_len);
		//��ȡ�ļ���range�ֿ���ļ�����
		path.read(&body[0], _range_len);
		if (!path.good())
		{
			std::cerr << "read file" << _file << "range data failed\n";
			_res = false;
			return;
		}
		path.close();

		//�ϴ�range����
		bf::path name(_file);
		//��֯�ϴ���uri·��   method url version
		// PUT /list/filename HTTP/1.1
		std::string url = BACKUP_URI + name.filename().string();
		//ʵ����һ��httplib�Ŀͻ��˶���
		httplib::Client cli(SERVER_IP, SERVER_PORT);
		//����http����ͷ��Ϣ
		httplib::Headers hdr;
		hdr.insert(std::make_pair("Conten-Length", std::to_string(_range_len)));
		std::stringstream tmp;
		tmp << "bytes=" << _range_start << "-" << (_range_start + _range_len - 1);
		hdr.insert(std::make_pair("Range", tmp.str().c_str()));
		//ͨ��ʵ������Client�����˷���PUT����
		auto rsp = cli.Put(url.c_str(), hdr, body, "text/plain");
		if (rsp && rsp->status != 200)
		{
			_res = false;
		}
		std::stringstream ss;
		ss << "backup file" << _file << "] range[" << _range_start << "-" << _range_len << "] backup success\n";
		std::cout << ss.str();
		return;
	}
};
class CloudClient
{
public:

	CloudClient()
	{
		bf::path file(CLIENT_BACKUP_DIR);
		if (!bf::exists(file))
		{
			bf::create_directory(file);
		}
	}

private:

	bool GetBackUpInfo()
	{
		bf::path path(CLIENT_BACKUP_INFO_FILE);
		if (!bf::exists(path))
		{
			std::cerr << "list file" << path.string() << "is not exist\n";
			return false;
		}
		int64_t fsize = bf::file_size(path);
		if (fsize == 0)
		{
			std::cerr << "have no backup\n";
			return false;
		}
		std::string body;
		body.resize(fsize);
		std::ifstream file(CLIENT_BACKUP_INFO_FILE, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "list file open error\n";
			return false;
		}
		file.read(&body[0], fsize);
		if (!file.good())
		{
			std::cerr << "read list file body error\n";
			return false;
		}
		file.close();
		std::vector<std::string> list;
		boost::split(list, body, boost::is_any_of("\n"));
		for (auto i : list)
		{
			//filename1 etg\n
			//filename2 etag\n
			size_t pos = i.find(" ");
			if (pos == std::string::npos)
			{
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			_backup_list[key] = val;
		}
		return true;
	}
	bool SetBackUpInfo()
	{
		std::string body;
		for (auto i : _backup_list)
		{
			body += i.first + " " + i.second + "\n";
		}
		std::ofstream file(CLIENT_BACKUP_INFO_FILE, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "open list file error\n";
			return false;
		}
		file.write(&body[0], body.size());
		if (!file.good())
		{
			std::cerr << "set backip info error";
			return false;
		}
		file.close();
		return true;
	}
	bool BackUpDirListen(const std::string &path)
	{
		bf::path file(path);
		bf::directory_iterator item_begin(file);
		bf::directory_iterator item_end;
		for (; item_begin != item_end; ++item_begin)
		{
			if (bf::is_directory(item_begin->status()))
			{
				BackUpDirListen(item_begin->path().string());
				continue;
			}
			if (FileIsNeedBackUp(item_begin->path().string()) == false)
			{
				continue;
			}
			std::cerr << "file:[" << item_begin->path().string() << "] needs backup\n";
			if (PutFileData(item_begin->path().string()) == false)
			{
				continue;
			}
			AddBackUpInfo(item_begin->path().string());
		}
		return true;
	}
	bool AddBackUpInfo(const std::string &file)
	{
		// etag = mtime(�ļ����һ���޸�ʱ��) - fsize(�ļ���С)
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			return false;
		}
		_backup_list[file] = etag;
		return true;
	}

	bool GetFileEtag(const std::string &file, std::string &etag)
	{

		bf::path path(file);
		if (!bf::exists(path))
		{
			std::cerr << "get file" << file << "etag error\n";
			return false;
		}
		int64_t fsize = bf::file_size(path);
		int64_t mtime = bf::last_write_time(path);
		std::stringstream tmp;
		tmp << std::hex << fsize << "-" << std::hex << mtime;
		etag = tmp.str();
		return true;
	}
	bool FileIsNeedBackUp(const std::string &file)
	{
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			return false;
		}
		auto it = _backup_list.find(file);
		if (it != _backup_list.end() && it->second == etag)  //����Ҫ����
		{
			return false;
		}
		return true;
	}
	static void thr_start(ThrBackUp *backup_info)
	{
		backup_info->Start();
		std::cout << "into thr_start\n";
		return;
	}
	bool PutFileData(const std::string &file)
	{
		//���߳��ϴ�������С���ļ����зֿ鴫��
		//ͨ����ȡ�ֿ鴫���Ƿ�ɹ��ж������ļ��Ƿ��ϴ��ɹ�
		//1.��ȡ�ļ���С
		int64_t fsize = bf::file_size(file);
		if (fsize <= 0)
		{
			std::cerr << "file" << file << "is no need to backup\n";
			return false;
		}
		//2.�����ܹ���Ҫ�ֶ��ٿ飬�õ�ÿ���С�Լ���ʼλ��
		//3.ѭ�������̣߳����߳����ϴ��ļ�����
		int count = (int)(fsize / RANGE_MAX_SIZE);
		std::vector<ThrBackUp> thr_res;
		std::vector<std::thread> thr_list;
		std::cerr << "file:[" << file << "] fisize:[" << fsize << "] count:[" << count + 1 << "] \n";
		for (int i = 0; i <= count; i++)
		{
			int64_t range_start = i * RANGE_MAX_SIZE;
			int64_t range_end = ((i + 1)*RANGE_MAX_SIZE) - 1;
			if (i == (count))
			{
				range_end = fsize -1;
			}
			int64_t range_len = range_end - range_start + 1;
			ThrBackUp backup_info(file, range_start, range_len);
			std::cerr << "file:[" << file << "] range:[" << range_start << "-" << range_end << "-" << range_len << "] \n";
			thr_res.push_back(backup_info);
		}
		for (int i = 0; i <= count; i++)
		{
			thr_list.push_back(std::thread(thr_start, &thr_res[i]));
		}
		//4.�ȴ������߳��˳����ж��ļ��ϴ����
		bool ret = true;
		for (int i = 0; i <= count; i++)
		{
			thr_list[i].join();   //ֻҪ��һ��ʧ�ܣ�����ʧ��
			if (thr_res[i]._res == true)
			{
				continue;
			}
			ret = false;
		}
		//5.�ϴ��ɹ���������ļ��ı�����Ϣ��¼
		if (ret == false)
		{
			return false;
		}
		std::cerr << "file:[" << file << "] backup success\n";
		return true;
	}

private:
	std::unordered_map<std::string, std::string> _backup_list;
public:
		bool Start()
		{
			GetBackUpInfo();
			while (1)
			{
				BackUpDirListen(CLIENT_BACKUP_DIR);
				SetBackUpInfo();
				Sleep(3000);
			}
			return true;
		}
};
