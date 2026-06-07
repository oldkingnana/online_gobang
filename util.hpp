#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <mysql/mysql.h>
#include <jsoncpp/json/json.h>
#include "myeasylog.hpp" // 引入你的手搓日志类


#ifndef UTIL_LOG
#define UTIL_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking
{
	// file_util 是静态文件读取工具,主要服务于 HttpRouter 的静态资源分发。
	// 它只负责从磁盘读取文件内容和解析文件扩展名,不理解 HTTP 业务含义。
	class file_util 
	{
	public:
		// 以二进制方式读取整个文件内容到 body 中,读取失败返回 false。
	    static bool read(const std::string &filename, std::string &body) 
		{
	        std::ifstream file;
	        
	        // 以只读,二进制方式打开文件
	        file.open(filename.c_str(), std::ios::in | std::ios::binary);
	        if (!file.is_open()) 
			{
	            UTIL_LOG(LOG_ERROR, "文件打开失败: " << filename);
	            return false;
	        }
	
	        // 游标拨到末尾 -> 获取大小 -> 申请内存 -> 游标拨回开头
	        file.seekg(0, std::ios::end);
	        body.resize(file.tellg());
	        file.seekg(0, std::ios::beg);
	
	        // 一次性将文件全部读入string的连续内存中
	        file.read(&body[0], body.size());
	
	        // 检查读取过程中是否发生错误
	        if (!file.good()) 
			{
	            UTIL_LOG(LOG_ERROR, "文件读取过程中发生异常: " << filename);
	            file.close();
	            return false;
	        }
	
	        file.close();
	        return true;
	    }

		// 从路径中提取文件扩展名,例如 /index.html 提取为 .html。
		static bool GetFileExten(const std::string& path, std::string& exten)
		{
		    auto query_pos = path.find('?');
		
		    std::string real_path =
		        (query_pos == std::string::npos)
		        ? path
		        : path.substr(0, query_pos);
		
		    auto dot_pos = real_path.rfind('.');
		
		    if(dot_pos == std::string::npos)
		        return false;
		
		    exten = real_path.substr(dot_pos);
		
		    return true;
		}
	};
	
	// 猜测会有线程安全问题(非原子的?),所以对于Json的辅助对象,我们临时申请而不是作为成员变量常驻
	// json_util 是 JSON 序列化/反序列化工具,用于 HTTP 和 WebSocket 在字符串与 Json::Value 之间转换。
	// 它每次调用都创建局部 reader/writer,避免把 JsonCpp 辅助对象作为全局共享状态。
	class json_util
	{
	public:
		// 将 Json::Value 序列化为紧凑 JSON 字符串。
	    static bool serialize(const Json::Value& src_json, std::string& dst_str)
	    {
	        Json::StreamWriterBuilder swb;
	        // 优化: 去掉格式化缩进和换行,压缩网络传输的体积
	        swb.settings_["indentation"] = ""; 
	        
	        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
	        std::stringstream ss;
	        
	        if (sw->write(src_json, &ss) != 0)
	        {
	            UTIL_LOG(LOG_ERROR, "JSON序列化失败!");
	            return false;
	        }
	        
	        dst_str = ss.str();
	        return true;
	    }
	
		// 将 JSON 字符串反序列化为 Json::Value,解析失败时记录错误日志。
	    static bool deserialize(const std::string& src_str, Json::Value& dst_json)
	    {
	        Json::CharReaderBuilder crb;
	        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
	        std::string errs;
	        
	        if (!cr->parse(src_str.c_str(), src_str.c_str() + src_str.size(), &dst_json, &errs))
	        {
	            UTIL_LOG(LOG_ERROR, "JSON反序列化失败! 原因: " << errs);
	            return false; 
	        }
	        
	        return true; 
	    }
	};

	// mysql_util 是 MySQL C API 的轻量封装,负责创建连接、执行 SQL、读取结果集并释放连接。
	// 它是底层数据库工具,不直接表达用户注册、胜负结算等业务含义;这些含义由 user_table 封装。
	class mysql_util 
	{
	public:
		// 创建并初始化 MySQL 连接,同时设置字符集为 utf8。
	    static MYSQL* create_mysql(const std::string& host, 
	               const std::string& user, 
	               const std::string& pass, 
	               const std::string& db, 
	               unsigned int port = 3306) 
	    {
	        MYSQL* mfp = mysql_init(nullptr);
	        if (mfp == nullptr)
	        {
	            UTIL_LOG(LOG_ERROR, "MySQL 初始化句柄失败!");
	        	return nullptr;
			}
	
	        if (!mysql_real_connect(mfp, host.c_str(), user.c_str(), pass.c_str(), 
	                                db.c_str(), port, nullptr, 0))
	        {
	            UTIL_LOG(LOG_ERROR, "MySQL 连接失败! 错误信息: " << mysql_error(mfp));
	            return nullptr;
	        }
	
	        mysql_set_character_set(mfp, "utf8");
	        UTIL_LOG(LOG_INFO, "MySQL 连接成功! 数据库: " << db);
	
			return mfp;
	    }
	
	    ~mysql_util() 
	    {}
	
		// 执行一条 SQL 语句,成功返回 true,失败时记录 MySQL 错误。
	    static bool execute(MYSQL* mfp, const std::string& sql)
	    {
	        if (mysql_query(mfp, sql.c_str()))
	        {
	            UTIL_LOG(LOG_ERROR, "SQL 执行失败! 错误信息: " << mysql_error(mfp) << " | SQL: " << sql);
	            return false;
	        }
	        return true;
	    }

		// 读取最近一次查询产生的结果集,并按列名组织成 Json::Value 数组。
		static bool store_line(MYSQL* mfp, std::vector<Json::Value>& usr_data)
		{
		    MYSQL_RES* res = mysql_store_result(mfp);
		
		    if (res == nullptr)
		    {
		        if (mysql_field_count(mfp) == 0)
		        {
		            UTIL_LOG(LOG_WARNING, "获取结果集为空! ");
		            return true;
		        }
		        else
		        {
		            UTIL_LOG(LOG_ERROR, "获取结果集失败! 错误信息: " << mysql_error(mfp));
		            return false;
		        }
		    }
		
		    auto row_num = mysql_num_rows(res);
		    auto col_num = mysql_num_fields(res);
		    MYSQL_FIELD* fields = mysql_fetch_fields(res);
		
		    std::vector<std::string> headers(col_num);
		    for (unsigned int i = 0; i < col_num; i++)
		    {
		        headers[i] = fields[i].name;
		    }
		
		    usr_data.clear();
		    usr_data.resize(row_num);
		
		    MYSQL_ROW line;
		    unsigned long* lengths = nullptr;
		
		    for (unsigned long i = 0; i < row_num; i++)
		    {
		        line = mysql_fetch_row(res);
		        lengths = mysql_fetch_lengths(res);
		
		        for (unsigned int j = 0; j < col_num; j++)
		        {
		            const std::string& key = headers[j];
		
		            if (line[j] == nullptr)
		            {
		                usr_data[i][key] = Json::nullValue;
		                continue;
		            }
		
		            std::string value(line[j], lengths[j]);
		
		            switch (fields[j].type)
		            {
		                case MYSQL_TYPE_TINY:
		                case MYSQL_TYPE_SHORT:
		                case MYSQL_TYPE_LONG:
		                case MYSQL_TYPE_INT24:
		                    usr_data[i][key] = std::stoi(value);
		                    break;
		
		                case MYSQL_TYPE_LONGLONG:
		                    usr_data[i][key] = Json::Int64(std::stoll(value));
		                    break;
		
		                case MYSQL_TYPE_FLOAT:
		                case MYSQL_TYPE_DOUBLE:
		                case MYSQL_TYPE_DECIMAL:
		                case MYSQL_TYPE_NEWDECIMAL:
		                    usr_data[i][key] = std::stod(value);
		                    break;
		
		                case MYSQL_TYPE_BIT:
		                    usr_data[i][key] = (value != "0");
		                    break;
		
		                default:
		                    usr_data[i][key] = value;
		                    break;
		            }
		        }
		    }
		
		    mysql_free_result(res);
		    return true;
		}

		// 释放 MySQL 连接句柄。
		void mysql_release(MYSQL* mfp)
		{
			if(mfp == nullptr)
				return ;

			mysql_close(mfp);
			return ;
		}
	};

	// 用于方便拆分用户数据或者其他的以任意分隔符为间隔的数据

	// string_util 是字符串辅助工具,当前主要用于按分隔符拆分 Cookie 等简单文本。
	class string_util {
	public:
	    // skip_empty用于过滤掉连续分隔符产生的空串
		// 按指定分隔符拆分字符串,可选择是否跳过空字段。
	    static int split(const std::string &in, const std::string &sep, std::vector<std::string> &arry, bool skip_empty = true) 
		{
	        arry.clear();
	
	        // 防死循环
	        if (sep.empty()) 
			{
	            arry.push_back(in);
	            return arry.size();
	        }
	
	        size_t pos, idx = 0;
	        while(idx < in.size()) 
			{
	            pos = in.find(sep, idx);
	            if (pos == std::string::npos) 
				{
	                // 处理最后一段
	                std::string tail = in.substr(idx);
	                if (!skip_empty || !tail.empty()) 
					{
	                    arry.push_back(tail);
	                }
	                break;
	            }
	            
	            // 处理中间切出来的段
	            if (pos == idx) 
				{
	                // 遇到了连续的分隔符
	                if (!skip_empty) 
					{
	                    arry.push_back(""); // 如果不跳过，就塞入空串
	                }
	            } 
				else 
				{
	                // 正常切出一段
	                arry.push_back(in.substr(idx, pos - idx));
	            }
	            
	            idx = pos + sep.size();
	        }
	        return arry.size();
	    }
	};
}
