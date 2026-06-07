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
	// user_table 是用户数据库表的访问封装,所有用户注册、登录校验、战绩更新都通过它访问 MySQL。
	// 它保存一个 MYSQL 连接句柄,对外提供按用户名/uid 查询用户、注册用户、校验登录、胜负结算等函数。
	// 当前它仍然是比较直接的 SQL 封装,没有做事务、连接池和 SQL 参数化;后续如果做正式结算,胜负更新应当合并成事务。
	// 业务层不应该直接拼 SQL 操作 user 表,而应该通过这个类集中管理用户数据访问逻辑。
	class user_table
	{
	public:
		// 创建用户表访问对象,连接指定 MySQL 数据库,连接失败则直接退出进程。
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
	
		// 按用户名查询用户信息,查询成功时把第一行用户数据写入 data。
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
		
		// 按 uid 查询用户信息,查询成功时把第一行用户数据写入 data。
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
		// 注册新用户:检查字段和用户名重复,再向 user 表插入初始战绩数据。
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
		// 校验用户登录信息,当前 HTTPServices 已经自行实现登录校验,这个函数保留为用户表层登录接口。
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
		// 记录用户胜利:增加总局数、胜场数和积分。
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
		// 记录用户失败:增加总局数、负场数并扣除积分。
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
