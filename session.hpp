#pragma once

#include <iostream>

#include "myeasylog.hpp"
#include "common.hpp"

#define SESSION_TIMEOUT 30000

/*
简单来说,为了保证长连接,也就是哪怕关闭客户端,也始终算作连接状态,于是有了cookie,很多网站进入的时候会询问你同不同意使用cookie
但是cookie本身是个数据载体,如果你直接存用户名和密码,就会导致安全性不太好,因为密码被储存在客户端,当用户被攻击之后,客户端的用户名和密码就可能会被盗用
于是有了session,session几乎就是一个临时通行凭证,依靠载体cookie,由服务器颁发,当用户首次登陆的时候,肯定需要向服务器提交用户名和密码,然后服务器会颁发这个凭证给客户端,客户端本地储存这个凭证,以后的一小段时间内,你就可以凭借这个临时的凭证访问服务器,就实现了长连接,至于安全性问题,凭证本身多半是一个毫无规律的字符串(哈希值?)所以即便保存在用户端,安全性也比cookie存密码要更好
如果session过期了,用户就没法访问服务器了
但是现在很多网页中,你会发现我可以一直保持登录状态非常久,除非十天半个月不登陆
我猜是服务器做了session更新,比方说有效期已经过半,用户依然登陆上了,那么此时对session做更新,也就是服务器重新颁发session给用户,客户端自己做更新
当然也有可能是服务端重置了计时
*/

#ifndef SS_LOG
#define SS_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking
{
	// 谁会来访问session?需不需要加锁?
	// session 描述一个用户在服务端的登录凭证,它和客户端 Cookie 中的 SSID 对应。
	// 它保存 uid、ssid、登录状态和最后访问时间,用于判断用户是否仍然保持登录态。
	// login/unlogin 状态不仅描述用户是否登录,也会影响 session 是否允许被删除和是否需要刷新访问时间。
	// session 本身只保存单个会话的数据和简单状态操作,批量管理、查找、删除由 session_manager 负责。
	class session 
	{
	public:
		typedef enum{login, unlogin} ss_stat;
		time_t last_access_time_; //惰性删除
	
	private:
		ssid_t ssid_;
		uid_t uid_;
		ss_stat stat_;

	public:
	
		// 创建一个会话对象,记录 ssid、uid、初始状态,并把最后访问时间初始化为当前时间。
		session(ssid_t ssid, uid_t uid, ss_stat stat)
		: last_access_time_(time(nullptr))
		, ssid_(ssid)
		, uid_(uid)
		, stat_(stat)
		{
			std::stringstream ss;
			ss << (void*)this;
			SS_LOG(LOG_INFO, "session[" + ss.str() + "]已创建");
		}

		// 销毁会话对象时写日志,用于观察 session 生命周期。
		~session()
		{
			// log
			std::stringstream ss;
			ss << (void*)this;
			SS_LOG(LOG_INFO, "session[" + ss.str() + "]已销毁");
		}

		// 获取 session 绑定的用户 uid。
		uid_t uid() { return uid_; }
		// 获取下发给客户端 Cookie 的 SSID。
		ssid_t ssid() { return ssid_; }
		// void update_ssid(ssid_t new_ssid) { ssid_ = new_ssid; }
		// 更新 session 登录状态,用于登录/登出等状态切换。
		void update_stat(ss_stat new_stat) { stat_ = new_stat; }
		// 判断 session 当前是否处于 login 状态。
		bool is_login() { return stat_ == ss_stat::login; }
		// 刷新最后访问时间,用于延长活跃用户的登录态。
		void update_time() { last_access_time_ = time(nullptr); }
		// 判断 session 是否超过 SESSION_TIMEOUT。
		bool is_expired() { return time(nullptr) - last_access_time_ > SESSION_TIMEOUT; }	
	};
}


