#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <unordered_map>
#include "myeasylog.hpp"
#include "mutex.hpp" 

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
    class online_manager
    {
        typedef websocketpp::server<websocketpp::config::asio> websocket_server;
        typedef uint64_t uid_t;
        typedef websocket_server::connection_ptr usr_conn_ptr;

    public:
        bool in_hall(uid_t uid)
        {
			// 锁自动释放
            oldking::lock_guard lock(mtx_);
            return game_hall_.count(uid) > 0;
        }

        bool in_room(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            return game_room_.count(uid) > 0;
        }

        void enter_hall(uid_t uid, usr_conn_ptr conn)
        {
            oldking::lock_guard lock(mtx_);
            
            if (game_hall_.count(uid)) 
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 重新进入大厅，覆盖旧连接");
            }
            game_hall_[uid] = conn;
            OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 成功进入游戏大厅");
        }

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

        void enter_room(uid_t uid, usr_conn_ptr conn)
        {
            oldking::lock_guard lock(mtx_);
            
            if (game_room_.count(uid)) 
			{
                OL_MNG_LOG(LOG_WARNING, "用户 [" + std::to_string(uid) + "] 重新进入房间，覆盖旧连接");
            }
            game_room_[uid] = conn;
            OL_MNG_LOG(LOG_INFO, "用户 [" + std::to_string(uid) + "] 成功进入房间");
        }

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

        usr_conn_ptr find_from_hall(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            auto it = game_hall_.find(uid);
            if (it != game_hall_.end()) 
			{
                return it->second;
            }
            return usr_conn_ptr();
        }

        usr_conn_ptr find_from_room(uid_t uid)
        {
            oldking::lock_guard lock(mtx_);
            auto it = game_room_.find(uid);
            if (it != game_room_.end()) 
			{
                return it->second;
            }
            return usr_conn_ptr();
        }

    private:
        oldking::mymutex mtx_; 
        std::unordered_map<uid_t, usr_conn_ptr> game_hall_;
        std::unordered_map<uid_t, usr_conn_ptr> game_room_;
    };
}

