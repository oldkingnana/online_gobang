#pragma once

#include <unordered_map>
#include "myeasylog.hpp"
#include "mutex.hpp" 
#include "common.hpp"
#include "net_common.hpp"

#ifndef OL_MNG_LOG
#define OL_MNG_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking
{
    // online_manager 负责维护“用户当前连接在哪个业务区域”的在线状态。
    // 它把 uid 映射到 WebSocket 连接,并分成大厅连接表 game_hall_ 和房间连接表 game_room_。
    // 大厅业务进入时调用 enter_hall,匹配成功进入房间时调用 enter_room,离开或断开时调用对应 exit。
    // 它不负责判断 session 是否合法,也不负责创建/删除房间;它只回答“这个 uid 当前在哪”和“给这个 uid 发消息应该找哪条连接”。
    // 这也是广播组件能通过 uid 找到对手连接的基础。
    class online_manager
    {
    public:
        // 判断用户是否登记在大厅连接表中。
        bool in_hall(uid_t uid)
        {
			// 锁自动释放
            oldking::lock_guard lock(mtx_);
            return game_hall_.count(uid) > 0;
        }

        // 判断用户是否登记在房间连接表中。
        bool in_room(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            return game_room_.count(uid) > 0;
        }

        // 将用户登记到大厅状态,如果已有旧连接则覆盖为当前连接。
        void enter_hall(uid_t uid, usr_conn_ptr_t conn)
        {
            oldking::lock_guard lock(mtx_);
            
            if (game_hall_.count(uid)) 
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 重新进入大厅，覆盖旧连接");
            }
            game_hall_[uid] = conn;
            OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 成功进入游戏大厅");
        }

        // 将用户从大厅状态移除,通常用于离开大厅、匹配成功进入房间或连接断开。
        void exit_hall(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            
            auto it = game_hall_.find(uid);
            if (it != game_hall_.end()) 
			{
                game_hall_.erase(it);
                OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 离开游戏大厅");
            }
			else
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 不在大厅中，离开大厅失败");
            }
        }

        // 将用户登记到房间状态,供房间内落子、聊天、广播等业务查找连接。
        void enter_room(uid_t uid, usr_conn_ptr_t conn)
        {
            oldking::lock_guard lock(mtx_);
            
            if (game_room_.count(uid)) 
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 重新进入房间，覆盖旧连接");
            }
            game_room_[uid] = conn;
            OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 成功进入房间");
        }

        // 将用户从房间状态移除,通常由退出房间、房间清理或连接断开触发。
        void exit_room(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            
            auto it = game_room_.find(uid);
            if (it != game_room_.end()) 
			{
                game_room_.erase(it);
                OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 离开房间");
            }
			else 
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 不在房间中，离开房间失败");
            }
        }

        // 从大厅连接表中查找用户连接,供大厅相关定向通知使用。
        usr_conn_ptr_t find_from_hall(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            auto it = game_hall_.find(uid);
            if (it != game_hall_.end()) 
			{
                return it->second;
            }
            return usr_conn_ptr_t();
        }

        // 从房间连接表中查找用户连接,供房间广播根据 uid 找到对手连接。
        usr_conn_ptr_t find_from_room(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            auto it = game_room_.find(uid);
            if (it != game_room_.end()) 
			{
                return it->second;
            }
            return usr_conn_ptr_t();
        }

    private:
        oldking::mymutex mtx_; 
        std::unordered_map<uid_t, usr_conn_ptr_t> game_hall_;
        std::unordered_map<uid_t, usr_conn_ptr_t> game_room_;
    };
}

