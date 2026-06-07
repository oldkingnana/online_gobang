#pragma once

#include <unordered_map>


#include "room.hpp"
#include "myeasylog.hpp"
#include "mutex.hpp"
#include "common.hpp"
#include "online_manager.hpp"
#include "user_table.hpp"


#ifndef RM_MNG_LOG
#define RM_MNG_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif


namespace oldking 
{
	// 房间管理器, 负责房间的组织管理和匹配操作(?)
	// room_manager 负责维护所有已经创建的房间对象,以及 uid 到 room_id 的归属关系。
	// matcher 匹配到两个大厅用户后调用 create_room 创建 room,并把双方 uid 记录到 users_ 映射中。
	// 房间业务收到落子、聊天、退出请求时,通过 get_room_by_uid 找到该用户所在房间,再把具体操作交给 room 执行。
	// 它不保存 WebSocket 连接,连接归 online_manager 管;它也不处理落子规则,规则归 room 管。
	// 当某个房间结束或一方退出时,server 层根据 cleanup 调用 delete_room 删除房间和 uid 归属映射。
	class room_manager
	{
	private:
		typedef std::shared_ptr<room> room_ptr;
		
		rid_t next_rid_;
		
		std::unordered_map<rid_t, room_ptr> rooms_;
		std::unordered_map<uid_t, rid_t> users_;
		
		online_manager* online_manager_p_;
		user_table* user_table_p_;

		oldking::mymutex mtx_;

	public:
		// 创建房间管理器,绑定 online_manager 用于确认玩家仍在大厅,绑定 user_table 用于传给 room 做结算。
		room_manager(online_manager* olp, user_table* utp)
		: next_rid_(0)
		, rooms_()
		, users_()
		, online_manager_p_(olp)
		, user_table_p_(utp)
		{
			// log 
			RM_MNG_LOG(LOG_INFO, "房间管理器创建成功");
		}

		// 销毁房间管理器并写日志。
		~room_manager()
		{
			// log 
			RM_MNG_LOG(LOG_INFO, "房间管理器销毁成功");
		}

		// 为两个大厅用户创建一间房,记录 room_id -> room 和 uid -> room_id 的映射。
		bool create_room(uid_t uid1, uid_t uid2)
		{
			oldking::lock_guard lock(mtx_);	
			// 检测用户在线状态

			if(!online_manager_p_->in_hall(uid1))
			{
				RM_MNG_LOG(LOG_ERROR, "用户: " + std::to_string(uid1) + "不在线");
				return false;	
			}
			
			if(!online_manager_p_->in_hall(uid2))
			{
				RM_MNG_LOG(LOG_ERROR, "用户: " + std::to_string(uid2) + "不在线");
				return false;	
			}

			// 初始化房间 	
			room_ptr room_p = std::make_shared<room>(next_rid_, uid1, uid2, user_table_p_);
		
			rooms_[next_rid_] = room_p;
			users_[uid1] = next_rid_;
			users_[uid2] = next_rid_;

			next_rid_++;
		
			return true;
		}

		// 根据房间号查找房间对象,供已知 room_id 的业务直接定位房间。
		room_ptr get_room_by_rid(rid_t rid)
		{
			oldking::lock_guard lock(mtx_);	
		
			if(rooms_.find(rid) == rooms_.end())
				return room_ptr();

			return rooms_[rid];
		}

		// 根据用户 uid 查找其所在房间,这是房间业务最常用的入口。
		room_ptr get_room_by_uid(uid_t uid)
		{
			oldking::lock_guard lock(mtx_);

			if(users_.find(uid) == users_.end())
				return room_ptr();

			rid_t rid = users_[uid];

			if(rooms_.find(rid) == rooms_.end())
				return room_ptr();
			
			return rooms_[rid];
		}


		// 删除指定房间,同时清理黑白双方 uid 到 room_id 的映射。
		bool delete_room(rid_t rid)
		{
			oldking::lock_guard lock(mtx_);	
		
			if(rooms_.find(rid) == rooms_.end())
				return false;	
			
			uid_t uid1 = rooms_[rid]->get_black_id();
			uid_t uid2 = rooms_[rid]->get_white_id();

			rooms_.erase(rid);
			users_.erase(uid1);
			users_.erase(uid2);

			return true;
		}
		
	
		

	};
}



