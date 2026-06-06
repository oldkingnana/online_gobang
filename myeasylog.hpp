#pragma once

// =======================INCLUDE=========================

#include <iostream>
#include <cstring>
#include <string>
#include <stdio.h>
#include <ctime>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <queue>
#include <memory>
#include <list>

#include "mutex.hpp"

namespace oldking
{
// =======================TYPEDEF=========================

	// type in log 
	typedef std::string log_level_t;
	typedef std::string log_time_t; 
	typedef std::string log_filename_t;
	typedef std::string log_process_id_t;
	typedef std::string log_thread_id_t;
	typedef std::string log_line_num_t;
	typedef std::string log_message_t;

// =======================DEFINE==========================

	// log level for my_easy_log
	#define LOG_DEBUG "DEBUG"  
	#define LOG_INFO "INFO"    
	#define LOG_WARNING "WARNING"
	#define LOG_ERROR "ERROR"
	#define LOG_FATAL "FATAL"

	// default log file path
	#define LOG_FILE "./log.txt"

// =======================BASE CLASSES====================

	// 基础日志类,用于构造单行日志
	struct BaseLog 
	{
		//BaseLog(const log_level_t& level, const log_filename_t& filename, const log_message_t& message)
		//: _time(GetTime())
		//, _level(level)
		//, _filename(filename)
		//, _pid(GetProcessID())
		//, _tid(GetThreadID())
		//, _line("NONE")
		//, _message(message)
		//{}

		BaseLog() = default;
		~BaseLog() = default;

		BaseLog(const BaseLog& other) = default;
		BaseLog& operator=(const BaseLog& other) = default;

		// 初始化 | 覆写
		void Init(const log_level_t& level, const log_filename_t& filename, const log_message_t& message)
		{
			_get_time_mutex.Lock();
			_time = GetTime();
			_get_time_mutex.Unlock();
			_level = level;
			_filename = filename;
			_pid = GetProcessID();
			_tid = GetThreadID();
			_line = "NONE";
			_message = message;
			_time.pop_back();
			_a_log = "[time]:" + _time +  
				    " [level]:" + _level + 
				    " [filename]:" + _filename + 
				    " [pid]:" + _pid + 
				    " [tid]:" + _tid + 
				    " [line]:" + _line + 
				    " [message]:" + _message;
		}

		void clear()
		{
			_time = "";
			_level = "";
			_filename = "";
			_pid = "";
			_tid = "";
			_line = "";
			_message = "";
			_a_log = "";
		}

		// 移动构造
		BaseLog(BaseLog&& other)
		{
			swap(other);	
		}
		// 移动赋值
		BaseLog& operator=(BaseLog&& other)
		{
			swap(other);
			return *this;
		}

		std::string& GetLog()
		{
			return _a_log;
		}

	private:	
		static log_time_t GetTime()
		{
			time_t time_buff = time(nullptr);
			struct tm* tm_info = localtime(&time_buff);
			char buffer[1024];
			strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
			
			return buffer;
			//return log_time_t(ctime(&time_buff));
		}

		static log_process_id_t GetProcessID()
		{
			return std::to_string(static_cast<int>(getpid()));
		}

		static log_thread_id_t GetThreadID()
		{
			return std::to_string(static_cast<unsigned long long>(pthread_self()));
		}

		void swap(BaseLog& other)
		{
			std::swap(_time, other._time);	
			std::swap(_level, other._level);	
			std::swap(_filename, other._filename);	
			std::swap(_pid, other._pid);	
			std::swap(_tid, other._tid);	
			std::swap(_line, other._line);	
			std::swap(_message, other._message);	
		}

	private:
		log_time_t _time;
		log_level_t _level;
		log_filename_t _filename;
		log_process_id_t _pid;
		log_thread_id_t _tid;
		log_line_num_t _line;
		log_message_t _message;
		std::string _a_log;

		oldking::mymutex _get_time_mutex;
	};
	
	// 策略基类 -- 用于规范后续设计的所有输出策略
	class LogOutputStrategy
	{
	public:
		LogOutputStrategy() = default;

		virtual ~LogOutputStrategy()
		{}
		
		virtual void Write(BaseLog* base_log_p,
				           const log_level_t& level,
				           const log_filename_t& filename, 
						   const log_message_t& message) = 0;
	
	};

	// 派生类 -- 日志文件输出模式
	class FileOutput : public LogOutputStrategy 
	{
	public:
		FileOutput(std::string file = LOG_FILE)
		: _file(file)
		{
			LoadFile();
		}

		~FileOutput() override
		{
			CloseFile();
		}

		void Write(BaseLog* base_log_p,
				   const log_level_t& level, 
				   const log_filename_t& filename, 
				   const log_message_t& message)		override
		{
			// init base_log
			base_log_p->Init(level, filename, message);
			
			// output to the file
			_file_mutex.Lock();
			fprintf(_plogfile, "%s \n", base_log_p->GetLog().c_str());
			fflush(_plogfile);
			_file_mutex.Unlock();
		}

	private:
		// 文件加载
		void LoadFile()
		{
			FILE* tmp = fopen(_file.c_str(), "a+");
			if(tmp != nullptr)
			{
				// std::cout << "LoadFile!" <<std::endl;
				_plogfile = tmp;
				return;
			}
			else
			{
				std::cerr << "fopen err: " << strerror(errno) << std::endl;
				exit(errno);
			}
		}

		// 文件释放
		void CloseFile()
		{
			if(fclose(_plogfile) != 0)
			{
				std::cerr << "fclose err: " << strerror(errno) << std::endl;
				exit(errno);
			}
		}

	private:
		FILE* _plogfile;
		std::string _file;
		oldking::mymutex _file_mutex;
	};

	// 派生类 -- 显示器输出模式
	class CmdOutput : public LogOutputStrategy 
	{
	public:
		CmdOutput() = default;

		~CmdOutput() override
		{}

		void Write(BaseLog* base_log_p,
				   const log_level_t& level, 
				   const log_filename_t& filename, 
				   const log_message_t& message)		override
		{
			// init base_log
			base_log_p->Init(level, filename, message);
			
			// output to the file
			_cmd_mutex.Lock();
			std::cout << base_log_p->GetLog() << std::endl;
			_cmd_mutex.Unlock();
		}
	private:
		oldking::mymutex _cmd_mutex;
	};

// =======================LOG CLASS=======================

	class MyEasyLog	
	{
	private:
		// 单例模式不允许外部构造,只允许内部构造
		MyEasyLog(size_t size)
		{
			_q_mutex.Lock();
			for(size_t i = 0; i < size; i++)
			{
				_void_log_q.push(new BaseLog);
			}
			_q_mutex.Unlock();

			SetOutputLogtoFile();
		}

		MyEasyLog(const MyEasyLog& other) = delete;
		MyEasyLog& operator=(const MyEasyLog& other) = delete;

	public: 
		~MyEasyLog()
		{
			_q_mutex.Lock();
			while(_void_log_q.size())
			{
				delete _void_log_q.front();
				_void_log_q.pop();
			}
			_q_mutex.Unlock();
		}

		// 外部的日志输出接口
		void WriteLog(const log_level_t& level, 
		              const log_filename_t& filename, 
                      const log_message_t& message)	
		{
			// get base log
			_q_mutex.Lock();
			BaseLog* baselog_p = nullptr;
			while(true)
			{
				if((baselog_p = _void_log_q.front()) != nullptr)
				{
					break;
				}
				else 
				{
					_q_mutex.Unlock();
					usleep(1000);
					_q_mutex.Lock();
				}
			}
			_void_log_q.pop();
			_q_mutex.Unlock();

			// output
			_file_mutex.Lock();
			_p_writer->Write(baselog_p, level, filename, message);
			_file_mutex.Unlock();

			// return base log to the queue 
			_q_mutex.Lock();
			_void_log_q.push(baselog_p);
			_q_mutex.Unlock();
		}

		// 显示器输出模式
		void SetOutputLogtoCmd()
		{
			// make_unique会帮我们new
			// 其本质是帮我们new完之后,返回一个右值指针
			// 然后智能指针通过移动赋值将新资源交换进来
			// 旧的资源会被make_unique释放
			_p_writer = std::make_unique<CmdOutput>();
		}

		// 日志文件输出模式
		void SetOutputLogtoFile()
		{
			_p_writer = std::make_unique<FileOutput>();
		}

		// 访问单例模式日志类的唯一接口,同时也是单词创建单例模式日志类的唯一接口
		static MyEasyLog& GetInstance()
		{
			static MyEasyLog myeasylog_instance(5);
			return myeasylog_instance;
		}
		//在这个例子中，这个logger类不能被外部构造，但内部构造却可以实现，因为巧用了peivate机制实现允许内部构造但不允许外部构造的机制
		//然后在其内部实现一个GetInstance方法，这个方法中，会构造一个instance对象，这个对象是全局的，静态的，但如果不调用Get方法，就不能初始化这个对象
		//但规定了，如果要使用这个对象，就只能先调用Get方法，然后通过其返回的引用使用该对象
		//于是就实现了该静态对象的强制初始化
		//如果再次调用Get方法试图使用instance时，因为已经被初始化过一次了，所以之后的代码中，用于初始化的那一行就会被忽略/不执行
		//同时，静态对象在CPP11中，其初始化是线程安全的，所以不用担心线程安全问题

	private:
		std::queue<BaseLog*, std::list<BaseLog*>> _void_log_q;
		oldking::mymutex _q_mutex;
		oldking::mymutex _file_mutex;
		// 智能指针用于帮助我们自动释放资源
		// 这里用智能指针包了一个基类,我们通过在派生类重写基类方法,实现了非常高的可扩展性
		// 以及尽可能保证输出方法和日志类解耦
		// 这里我们用了策略模式,将输出方式实现为策略类
		// 且全部的策略全部继承自基础策略抽象类(用于规范策略类需要重写的接口)
		// 在日志类中,我们不关心使用怎样的输出策略
		// 而在策略类中,除了baselog之外,我们也不关心其他任何日志类的成员,仅做输出
		// 实现了一种弱解耦的状态
		// 是开闭原则的非常好的体现(开闭原则指高扩展性,却不可修改类的内部实现)
		// 将代码测试工作变得更为简单,我们只需要测试新增加的策略,原来的类就不用测试了
		std::unique_ptr<LogOutputStrategy> _p_writer;
	};

}


//namespace oldking
//{
//	typedef std::string log_level_t;
//	#define SIMP_LEVEL "SIMPLE"
//	#define WARN_LEVEL "WARNNING"
//	#define ERR_LEVEL "ERROR"
//
//	class my_easy_log
//	{
//	public:
//		// my_easy_log(const int& max_line, const std::string& file)
//		my_easy_log(const std::string& file)
//		// _max_line(max_line)
//		: _linecnt(0)
//		, _plogfile(nullptr)
//		, _file(file)
//		{}
//
//		bool LoadFile()
//		{
//			FILE* tmp = fopen(_file.c_str(), "a+");
//			if(tmp != nullptr)
//			{
//				std::cout << "LoadFile!" <<std::endl;
//				_plogfile = tmp;
//				return true;
//			}
//			else
//			{
//				std::cerr << "fopen err: " << strerror(errno) << std::endl;
//				return false;
//			}
//		}
//
//		void Write(const std::string& message, const log_level_t& level)
//		{
//			// std::cout << "Write!" << std::endl;
//
//			if(_plogfile == nullptr)
//			{
//				std::cerr << "_plogfile is nullptr" << std::endl;
//				return ;
//			}
//
//			time_t now = time(nullptr);
//			struct tm* tm_info = localtime(&now);
//
//			char buffer[64];
//			strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
//
//			fprintf(_plogfile, "time: %s ,    level: %s ,   message: %s\n", buffer, level.c_str(), message.c_str());
//			_linecnt++;
//		}
//
//		// void SetMax(const unsigned long long& max_number)
//		// {
//		// 	_max_line = max_number;
//		// }
//
//		unsigned long long GetLineCount()
//		{
//			return _linecnt;
//		}
//
//		~my_easy_log()
//		{
//			if(_plogfile == nullptr) return ;
//			fclose(_plogfile);
//		}
//
//	private:
//		// const unsigned long long _max_line;
//		unsigned long long _linecnt;
//		FILE* _plogfile;
//		std::string _file;
//	};
//}
//
//







