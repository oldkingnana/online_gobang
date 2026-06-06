#include "session_manager.hpp"

namespace oldking 
{
	// 本质上这个鉴权组件就是一个过滤器,专门用于过滤前端用户操作信息中,关于身份验证的内容
	// 或者说,不能通过鉴权就不能获取uid,那么这个用户就是非法访问,不做处理
	// 如果能拿到uid给下级组件,那么这个用户就是合法访问
	
	struct auth_result 
	{
	private:
		typedef uint64_t uid_t;
	public:
		bool ok;
		uid_t uid;
		std::string reason;
	};


	class session_detector 
	{
	private:
		typedef uint64_t uid_t;
		typedef std::string ssid_t;

		std::shared_ptr<session_manager> session_manager_ptr_;
	public:
		session_detector(std::shared_ptr<session_manager> smp)
		: session_manager_ptr_(smp)
		{}

		// 检测session合法性并通过session获取uid
		auth_result check_http (ssid_t ssid)
		{
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

		}
	};
}
