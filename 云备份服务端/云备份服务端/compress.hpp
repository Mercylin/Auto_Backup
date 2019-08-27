#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <thread>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <sys/stat.h>   //��ȡ���һ�η���ʱ��
#include <unistd.h>
#include <fcntl.h>
#include <zlib.h>
#include <pthread.h>
#include <sys/file.h>

#define GZIPFILE_PATH "www/zip/"
#define UNGZIPFILE_PATH "www/list/"
#define RECORD_FILE "record.list"   //�ļ���Ϣ�ļ�¼
#define HEAT_TIME 10  //�ȶ�ʱ��

namespace bf = boost::filesystem;

class CompressStore {
private:
	std::string _file_dir;                   //
	std::unordered_map<std::string, std::string> _file_list;   //���ڱ����ļ��б�
	pthread_rwlock_t _relock;
private:
	//1��ÿ��ѹ���洢�߳�������ʱ�򣬴��ļ��ж�ȡ�б���Ϣ
	bool GetListRecord()
	{
		//filename gzipfilename \n
		bf::path name(RECORD_FILE);
		if (!bf::exists(name))
		{
			std::cerr << "record file is not exists\n";
			return false;
		}

		std::ifstream file(RECORD_FILE, std::ios::binary);
		if (!file.is_open())
		{
			std::cerr << "open recordfile read error\n";
			return false;
		}
		int64_t fsize = bf::file_size(name);
		std::string body;
		body.resize(fsize);
		file.read(&body[0], fsize);
		if (!file.good())
		{
			std::cerr << "record file body read error\n";
			return false;
		}
		file.close;

		std::vector<std::string> list;
		boost::split(list,body,boost::is_any_of("\n"));
		for (auto i : list)
		{
			size_t pos = i.find(" ");
			if (pos == std::string::npos)
			{
					continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			_file_list[key] = val;
		}
		return true;
	}
	//2��ÿ��ѹ���洢��ϣ���Ҫ���б���Ϣ���洢���ļ���
	bool SetListRecord()
	{
		std::stringstream tmp;
		for (auto i : _file_list)
		{
			tmp << i.first << " " << i.second << "\n";
		}
		std::ofstream file(RECORD_FILE,std::ios::binary|std::ios::trunc);
		if (!file.is_open())
		{
			std::cerr << "record file open error\n";
			return false;
		}
		file.write(tmp.str().c_str(), tmp.str().size());
		if (!file.good())
		{
			std::cerr << "record file write body error\n";
			return false;
		}
		file.close;
		return true;
	}
	//2.1����ȡlistĿ¼���ļ�������
	bool DirdctoryCheck()
	{
		if (!bf::exists(UNGZIPFILE_PATH))     //�ж�����ļ��Ƿ���ڣ��������򴴽�
		{
			bf::create_directory(UNGZIPFILE_PATH);
		}
		bf::directory_iterator item_begin(UNGZIPFILE_PATH);
		bf::directory_iterator item_end;
		for (; item_begin != item_end; ++item_begin)
		{
			if (bf::is_directory(item_begin->status()))   //�ж��Ƿ���Ŀ¼
			{
				continue;   //��Ŀ¼������
			}
			std::string name = item_begin->path().string();   //��ȡ���ļ���
			std::string gzip = GZIPFILE_PATH + item_begin->path().filename().string() + ".gz";
			if (IsNeedCompress(name))            
			{
				std::string gzip = GZIPFILE_PATH + item_begin->path().filename().string() + ".gz";
				//std::string gzip = name + ".gz";
				CompressFile(name, gzip);
				AddFileRecord(name, gzip);
			}
		}
		return true;
	}
	//2.2���ж��ļ��Ƿ���Ҫѹ���洢
	bool IsNeedCompress(std::string &file)
	{
		struct stat st;
		if (stat(file.c_str, &st) < 0)
		{
			std::cerr << "get file:[" << file << "] stat error\n";
			return false;
		}
		time_t cur_time = time(NULL);
		time_t acc_time = st.st_atime;  //���һ�η���ʱ��
		if ((cur_time - acc_time) < HEAT_TIME)
		{
			return false;
		}

		return true;

	}
	//2.3�����ļ�����ѹ���洢
	bool CompressFile(std::string &file,std::string &gzip)
	{
		int fd = open(file.c_str(), O_RDONLY);
		if (fd < 0)
		{
			std::cerr << "com open file:[" << file << "] error\n";
			return false;
		}
		//std::string gzfile = file + ".gz";
		gzFile gf = gzopen(gzip.c_str(), "wb");
		if (gf == NULL)
		{
			std::cerr << "com open gzip:[" << gzip << "] error\n";
			return false;
		}
		int ret;
		char buf[1024];
		flock(fd, LOCK_SH);
		while ((ret = read(fd, buf, 1024)) > 0)    //��ȡ����ѹ��
		{
			gzwrite(gf,buf,ret);
		}
		flock(fd, LOCK_UN);
		close(fd);
		gzclose(gf);
		unlink(file.c_str());  //ɾ����ͨԭĿ¼�������ڱ�������ɾ������
		return true;
	}
	//3�����ļ����н�ѹ��
	bool UnCompressFile(std::string &gzip, std::string &file)
	{
		int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0664);
		if (fd < 0)
		{
			std::cerr << "open file" << file << "failed\n";
			return false;
		}
		gzFile gf = gzopen(gzip.c_str(), "rb");
		if (gf == NULL)
		{
			std::cerr << "open gzip" << gzip << "failed\n";
			close(fd);
			return false;
		}
		int ret;
		char buf[1024] = { 0 };
		flock(fd, LOCK_EX);
		while ((ret = gzread(gf, buf, 1024)) > 0)
		{
			int len = write(fd, buf, ret);
			if (len < 0)
			{
				std::cerr << "get gzip data failed\n";
				gzclose(gf);
				close(fd);
				flock(fd, LOCK_UN);
				return false;
			}
		}
		flock(fd, LOCK_UN);
		gzclose(gf);
		close(fd);
		unlink(gzip.c_str());
		return true;
	}
	//4���ж��ļ��Ƿ��Ѿ�ѹ��
	//bool IsCompressd(std::string &file);
	bool GetNormalFile(std::string &name, std::string &body)
	{
		int64_t fsize = bf::file_size(name);
		body.resize(fsize);
		int fd = open(name.c_str(), O_RDONLY);
		if (fd < 0)
		{
			std::cerr << "open file " << name << "failed\n";
			return false;
		}
		flock(fd, LOCK_SH);
		int ret = read(fd, &body[0],fsize);
		flock(fd, LOCK_UN);
		int(ret != fsize)
		{
			std::cerr << "get file " << name << "body error\n";
			close(fd);
			file.close();
			return false;
		}
		close(fd);
		return true;
	}
	bool AddFileRecord(const std::string file,const std::string &gzip)
	{
		pthread_rwlock_wrlock(&_rwlock);
		_file_list[file] = gzip;
		pthread_rwlock_unlock(&_rwlock);

	}
	bool GetFileGzip(std::string &file, std::string &gzip)  //ͨ���ļ����ƣ���ȡ��Ӧ��ѹ��������
	{
		pthread_rwlock_rdlock(&_rwlock);
		auto it = _file_list.find(file);
		if (it == _file_list.end())
		{
			pthread_rwlock_unlock(&_rwlock);
			return false;
		}
		gzip = it->second;
		pthread_rwlock_unlock(&_rwlock);
		return true;
	}
	
public:
	CompressStore()
	{
		pthread_rwlock_init(&_rwlock,NULL);
		if (!bf::exists(GZIPFILE_PATH))
		{
			bf::create_directory(GZIPFILE_PATH);
		}
	}
	~CompressStore()
	{
		pthread_rwlock_destory(&_rwlock);
	}

	bool GetFileList(std::vector<std::string> &list)           //�����ṩ��ȡ�ļ��б���
	{
		//����map
		//��д��
		pthread_rwlock_rdlock(&_rwlock);
		for (auto i : _file_list)
		{	
			list.push_back(i.first);
		}
		pthread_rwlock_unlock(&_rwlock);
		return true;
	}
	

	bool GetFileData(std::string &file, std::string &body)        //�����ṩ��ȡ�ļ����ݹ���
	{
	
		if (bf::exists(file))
		{
			//1.��ѹ���ļ����ݻ�ȡ
			GetNormalFile(file, body);
		}
		else
		{
			//2.ѹ���ļ������ݻ�ȡ
			//��ȡѹ���� gzip
			std::string gzip;
			GetFileGzip(file, gzip);
			UnCompressFile(gzip,file);
			GetNormalFile(file, body);
		}
	
	}
	
	bool SetFileData(const std::string &file,const std::string &body,const int64_t offset)
	{
		int fd = open(file.c_str(), O_CREAT | O_WRONLY, 0644);
		if (fd < 0)
		{
			std::cerr << "open file" << file << "error\n";
			return false;
		}
		flock(fd, LOCK_EX);
		lseek(fd,offset, body, SEEK_SET);
		int ret = write(fd, &body[0], body.size());
		if (ret < 0)
		{
			std::cerr << "store file" << file << "data error\n";
			flock(fd, LOCK_UN);
			return false;
		}
		flock(fd, LOCK_UN);
		close(fd);
		AddFileRecord(file,"");
		return true;
	}
	//��Ϊѹ���洢������ʱ��ѭ���������Ҫ�����߳�
	bool LowHeatFileStore()//�ȶȵ͵��ļ�����ѹ���洢
	{
		//1����ȡ��¼��Ϣ
		GetListRecord();
		while (1)
		{
				//2��Ŀ¼��⣬�ļ�ѹ���洢
			      //2.1����ȡlistĿ¼���ļ�������
				  //2.2���ж��ļ��Ƿ���Ҫѹ���洢
				  //2.3�����ļ�����ѹ���洢
				//3���洢��¼��Ϣ
			DirdctoryCheck();
			SetListRecord();
			sleep(3);
		}
	}
};