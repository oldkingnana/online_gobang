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

		~room_manager()
		{
			// log 
			RM_MNG_LOG(LOG_INFO, "房间管理器销毁成功");
		}

		bool create_room(uid_t uid1, uid_t uid2)
		{
			oldking::lock_guard lock(mtx_);	
			// 检测用户在线状态

			if(!online_manager_p_->in_hall(uid1))
			{
				RM_MNG_LOG(LOG_ERROR, "用户: " + std::to_string(uid1) + "不在线");
				return false;	
			}
			
			if(!online_manager_p_->in_hall(uid1))
			{
				RM_MNG_LOG(LOG_ERROR, "用户: " + std::to_string(uid2) + "不在线");
				return false;	
			}

			// 调整用户位置
			auto uid1_conn_ptr = online_manager_p_->find_from_hall(uid1);
			online_manager_p_->exit_hall(uid1);
			online_manager_p_->enter_room(uid1, uid1_conn_ptr);

			auto uid2_conn_ptr = online_manager_p_->find_from_hall(uid2);
			online_manager_p_->exit_hall(uid2);
			online_manager_p_->enter_room(uid2, uid2_conn_ptr);
			
			// 初始化房间 	
			room_ptr room_p = std::make_shared<room>(next_rid_, uid1, uid2, user_table_p_);
		
			rooms_[next_rid_] = room_p;
			users_[uid1] = next_rid_;
			users_[uid2] = next_rid_;

			next_rid_++;
		
			return true;
		}

		room_ptr get_room_by_rid(rid_t rid)
		{
			oldking::lock_guard lock(mtx_);	
		
			if(rooms_.find(rid) == rooms_.end())
				return room_ptr();

			return rooms_[rid];
		}

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



