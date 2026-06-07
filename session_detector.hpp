#pragma once

#include "common.hpp"
#include "session_manager.hpp"

namespace oldking 
{
	// 本质上这个鉴权组件就是一个过滤器,专门用于过滤前端用户操作信息中,关于身份验证的内容
	// 或者说,不能通过鉴权就不能获取uid,那么这个用户就是非法访问,不做处理
	// 如果能拿到uid给下级组件,那么这个用户就是合法访问
	
	struct auth_result 
	{
	public:
		bool ok;
		uid_t uid;
		std::string reason;
	};


	class session_detector 
	{
	private:
		std::shared_ptr<session_manager> session_manager_ptr_;

		auth_result check(ssid_t ssid)
		{
			auth_result result;
			result.ok = false;
			result.uid = 0;

			if(session_manager_ptr_ == nullptr)
			{
				result.reason = "session manager is null";
				return result;
			}

			if(ssid.empty())
			{
				result.reason = "ssid is empty";
				return result;
			}

			auto session_ptr = session_manager_ptr_->get_session(ssid);
			if(session_ptr == nullptr)
			{
				result.reason = "session does not exist";
				return result;
			}

			if(session_ptr->is_expired())
			{
				session_manager_ptr_->rm_session(ssid);
				result.reason = "session is expired";
				return result;
			}

			if(!session_ptr->is_login())
			{
				result.reason = "session is not login";
				return result;
			}

			if(time(nullptr) - session_ptr->last_access_time_ > (SESSION_TIMEOUT / 2))
			{
				session_ptr->update_time();
			}

			result.ok = true;
			result.uid = session_ptr->uid();
			result.reason = "session is valid";
			return result;
		}
	public:
		session_detector(std::shared_ptr<session_manager> smp)
		: session_manager_ptr_(smp)
		{}

		// 检测session合法性并通过session获取uid
		auth_result check_http (ssid_t ssid)
		{
			return check(ssid);
			// 在session_manager中检查是否存在对应session
			// 如果不存在,设置auth_result并返回
			
			
			// 如果存在session,获取ssid对应的session
			// 检查session是否超时,如果时限过半,更新session(这里选择把更新逻辑写在里面,外部就不必关心更新)
			// 如果超时,设置auth_result并返回
			
			// session完全合法,设置auth_result并返回 

		}

		// ws也是几乎一样的逻辑
		auth_result check_ws (ssid_t ssid)
		{
			return check(ssid);
		}
	};
}
