#pragma once

#include <unordered_map>
#include <random>

#include "session.hpp"
#include "mutex.hpp"
#include "myeasylog.hpp"
#include "common.hpp"

#ifndef SS_MNG_LOG
#define SS_MNG_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking 
{
	// 估计可以用定长内存池之类的优化一下
	// 惰性删除必须在上一级实现!!
	// session_manager 负责管理所有服务端 session,维护两套映射:ssid -> session 和 uid -> ssid。
	// 登录/注册成功时由它创建 session,后续 HTTP/WS 鉴权时由它根据 ssid 或 uid 找到已有 session。
	// 它不主动启动清理线程,过期删除采用惰性方式,由 session_detector 或登录逻辑在访问时发现过期并删除。
	// 因为它是登录态的核心共享组件,内部操作都用互斥锁保护;切换状态时直接在锁内查表,避免再次调用 get_session 造成自死锁。
	class session_manager 
	{
	private:
		typedef std::shared_ptr<session> ss_ptr_t;

		std::unordered_map<ssid_t, ss_ptr_t> sessions_;
		std::unordered_map<uid_t, ssid_t> ssids_;

		oldking::mymutex mtx_;

	// 创建一个随机 SSID 字符串,用于作为客户端 Cookie 中的会话凭证。
	ssid_t create_ssid()
	{
		// 从硬件获取随机数
		std::random_device rd;
		// 作为种子丢给梅森旋转算法
		std::mt19937_64 gen(rd());
		// 用于映射随机数到[0, 255],也就是1字节
		std::uniform_int_distribution<int> dis(0, 255);
	
		std::stringstream ss;
		for(int i = 0; i < 16; i++)
		{
			// 每次向字符串流中塞入一个两位,自动补全0且为16进制的字符串
			ss << std::setw(2) << std::setfill('0') << std::hex << dis(gen);
		}

		return ss.str();
	}

	public:
		// 创建 session 管理器并写日志。
		session_manager()
		{
			SS_MNG_LOG(LOG_INFO, "session管理器创建完成!");
		}
		
		// 销毁 session 管理器并写日志。
		~session_manager()
		{
			SS_MNG_LOG(LOG_INFO, "session管理器销毁完成!");
		}

		// 为指定 uid 创建新的 session,建立 uid 和 ssid 的双向查找关系,并返回新 ssid。
		ssid_t create_session(uid_t uid, oldking::session::ss_stat stat = oldking::session::login)
		{
			oldking::lock_guard lock(mtx_);

			// uid合法性检测,有没有必要?

			// 随机一个ssid
			ssid_t new_ssid = create_ssid();
			// 如果这个ssid不存在,则合法
			while(sessions_.find(new_ssid) != sessions_.end())
				new_ssid = create_ssid();	

			ss_ptr_t new_session = std::make_shared<session>(new_ssid, uid, stat);
	
			ssids_[uid] = new_ssid;
			sessions_[new_ssid] = new_session;

			return new_ssid;
		}
		
		// 根据 uid 获取对应 session,用于登录时复用已有会话。
		ss_ptr_t get_session(uid_t uid)
		{
			oldking::lock_guard lock(mtx_);
			
			if(ssids_.find(uid) == ssids_.end())
				return ss_ptr_t();

			return sessions_[ssids_[uid]];
		}
		
		// 根据 ssid 获取对应 session,用于鉴权组件从 Cookie 凭证查找登录态。
		ss_ptr_t get_session(ssid_t ssid)
		{
			oldking::lock_guard lock(mtx_);
			
			if(sessions_.find(ssid) == sessions_.end())
				return ss_ptr_t();

			return sessions_[ssid];
		}

		// 删除session(session必须离线)
		// 根据 ssid 删除 session,同时移除 uid -> ssid 映射。
		bool rm_session(ssid_t ssid)
		{
			oldking::lock_guard lock(mtx_);
		
			if(sessions_.find(ssid) == sessions_.end())
				return false;

			if(!sessions_[ssid]->is_login())
				return false;

			ssids_.erase(sessions_[ssid]->uid());
			sessions_.erase(ssid);

			return true;
		}

		// 删除函数重载
		// 根据 uid 删除 session,同时移除 ssid -> session 映射。
		bool rm_session(uid_t uid)
		{
			oldking::lock_guard lock(mtx_);
		
			if(ssids_.find(uid) == ssids_.end())
				return false;
			
			if(!sessions_[ssids_[uid]]->is_login())
				return false;

			sessions_.erase(ssids_[uid]);
			ssids_.erase(uid);

			return true;
		}
		
		// 这里可以设计一个接口用于切换session在线状态
		// 根据 uid 切换 session 状态,例如从 login 改为 unlogin 或恢复 login。
		bool switch_stat(uid_t uid, oldking::session::ss_stat stat)
		{
			oldking::lock_guard lock(mtx_);
			
			if(ssids_.find(uid) == ssids_.end())
				return false;

			ssid_t ssid = ssids_[uid];

			if(sessions_.find(ssid) == sessions_.end())
				return false;
		
			sessions_[ssid]->update_stat(stat);

			return true;
		}
		
		// 根据 ssid 切换 session 状态,供已经拿到 Cookie 凭证的调用方直接更新状态。
		bool switch_stat(ssid_t ssid, oldking::session::ss_stat stat)
		{
			oldking::lock_guard lock(mtx_);
			
			if(sessions_.find(ssid) == sessions_.end())
				return false;
			
			sessions_[ssid]->update_stat(stat);

			return true;
		}
	};
}

