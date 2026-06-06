#pragma once

#include <jsoncpp/json/json.h>
#include <string>

#include "util.hpp"
#include "myeasylog.hpp"


#ifndef TABLE_LOG
#define TABLE_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif


namespace oldking
{
	class user_table
	{
	public:
		user_table(const std::string& host, const std::string& user, const std::string& pass, const std::string& db)
		{
			mfp_ = oldking::mysql_util::create_mysql(
					host,
					user, 
					pass, 
					db);

			if(mfp_ == nullptr) // err
			{
				TABLE_LOG(LOG_ERROR, "创建mysql连接错误");
				exit(1);
			}
		}
	
		bool slct_by_name(const std::string& usr_name, Json::Value& data)
		{
			if(!oldking::mysql_util::execute(mfp_, "select * from user where usr_name=\'" + usr_name + "\'"))
			{
				TABLE_LOG(LOG_ERROR, "查询失败! ");
				return false;
			}
	       	
			std::vector<Json::Value> usr_data;

			oldking::mysql_util::store_line(mfp_, usr_data);

			if(usr_data.empty())
				return false;

			data = usr_data[0];

			return true;
		}
		
		bool slct_by_uid(const uint64_t& uid, Json::Value& data)
		{
			if(!oldking::mysql_util::execute(mfp_, "select * from user where uid=" + std::to_string(uid)))
			{
				TABLE_LOG(LOG_ERROR, "查询失败! ");
				return false;
			}
	       	
			std::vector<Json::Value> usr_data;

			oldking::mysql_util::store_line(mfp_, usr_data);

			if(usr_data.empty())
				return false;

			data = usr_data[0];

			return true;
		}
	
		// 必须要有usr_name和passwd
		bool rgst(const Json::Value& data)
		{
			// data合法性检查
			
			if(!(data["usr_name"] && data["passwd"]))
			{
				TABLE_LOG(LOG_ERROR, "data不合法!");
				return false;
			}
			
			// 重复用户检测?

			Json::Value select_result;
			if(slct_by_name(data["usr_name"].asString(), select_result))
			{
				TABLE_LOG(LOG_ERROR, "用户数据已经存在,用户已经注册!");
				return false;
			}
			
			// 没有该用户,进行注册
			bool execute_result = oldking::mysql_util::execute
				(mfp_, 
				std::string() + "insert into user(usr_name, passwd, game_cnt, win_cnt, los_cnt, point) values (" + "\'" + data["usr_name"].asString() + "\'" + ", " + "\'" + data["passwd"].asString() + "\'" + ", 0, 0, 0, 0)");	
		
			if(execute_result == false)
			{
				TABLE_LOG(LOG_ERROR, "SQL语句错误,执行SQL语句失败");
				return false;
			}

			return true;
		}

		// 必须要有usr_name和passwd
		bool login(const Json::Value& data)
		{
			// data合法性检查
			
			if(!(data["usr_name"] && data["passwd"]))
			{
				TABLE_LOG(LOG_ERROR, "data不合法!");
				return false;
			}
			
			// 用户检索	
			Json::Value select_result;
			if(!slct_by_name(data["usr_name"].asString(), select_result))
			{
				TABLE_LOG(LOG_ERROR, "查询失败!用户不存在!");
				return false;
			}

			if(select_result["passwd"] == data["passwd"])
			{
				TABLE_LOG(LOG_ERROR, "用户存在,但密码不正确!");
				return false;
			}
		
			return true;
		}
	
		// 必须要有uid
		bool win(const Json::Value& data)
		{
			// data合法性检查
			
			if(!data["uid"])
			{
				TABLE_LOG(LOG_ERROR, "data不合法!");
				return false;
			}
			
			// 用户检索	
			Json::Value select_result;
			if(!slct_by_uid(data["uid"].asInt(), select_result))
			{
				TABLE_LOG(LOG_ERROR, "查询失败!用户不存在!");
				return false;
			}
			
			bool execute_result = oldking::mysql_util::execute
				(mfp_, 
				std::string() + "UPDATE user SET game_cnt = game_cnt + 1, win_cnt = win_cnt + 1, point = point + 30 WHERE uid=" + data["uid"].asString());	
		
			if(execute_result == false)
			{
				TABLE_LOG(LOG_ERROR, "SQL语句错误,执行SQL语句失败");
				return false;
			}	

			return true;
		}
	
		// 必须要有uid
		bool lose(const Json::Value& data)
		{
			// data合法性检查
			
			if(!data["uid"])
			{
				TABLE_LOG(LOG_ERROR, "data不合法!");
				return false;
			}
			
			// 用户检索	
			Json::Value select_result;
			if(!slct_by_uid(data["uid"].asInt(), select_result))
			{
				TABLE_LOG(LOG_ERROR, "查询失败!用户不存在!");
				return false;
			}
			
			bool execute_result = oldking::mysql_util::execute
				(mfp_, 
				std::string() + "UPDATE user SET game_cnt = game_cnt + 1, los_cnt = los_cnt + 1, point = point - 5 WHERE uid=" + data["uid"].asString());	
		
			if(execute_result == false)
			{
				TABLE_LOG(LOG_ERROR, "SQL语句错误,执行SQL语句失败");
				return false;
			}	

			return true;
		}
	
	private:
		MYSQL* mfp_;
	};
}
