#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include "mutex.hpp"
#include "myeasylog.hpp"
#include "user_table.hpp"
#include "online_manager.hpp"

#ifndef ROOM_LOG
#define ROOM_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking 
{
	#define BOARD_ROW 15
	#define BOARD_COL 15
	#define CHESS_EMPTY 0
	#define CHESS_BLACK 1 
	#define CHESS_WHITE 2
	
	typedef enum{start, gameover} room_stat;

	// 这是专门负责用户操作的组件,或者说用于描述房间内操作,状态/数据只能经过上层管理组件
	class room
	{
	private:
		typedef websocketpp::server<websocketpp::config::asio> web_server;
		typedef web_server::connection_ptr usr_conn;

		typedef uint64_t uid_t; 
	
		typedef uint64_t rid_t;

		rid_t room_id_;

		std::vector<std::vector<int>> board_;
		
		uid_t white_id_;
		uid_t black_id_;
		uid_t current_player_;

		room_stat stat_;

		oldking::user_table* user_table_p_;

		oldking::mymutex mtx_;
		
		

		bool check_win(int row, int col, int color) 
		{
			// 方向
        	int dx[4] = {0, 1, 1, -1};
        	int dy[4] = {1, 0, 1, 1};

        	// 放射状探测
        	for (int i = 0; i < 4; ++i) 
			{
        	    int count = 1;

        	    int x = row + dx[i];
        	    int y = col + dy[i];
        	    while (x >= 0 && x < 15 && y >= 0 && y < 15 && board_[x][y] == color) {
        	        count++;
        	        x += dx[i];
        	        y += dy[i];
        	    }

        	    x = row - dx[i];
        	    y = col - dy[i];
        	    while (x >= 0 && x < 15 && y >= 0 && y < 15 && board_[x][y] == color) {
        	        count++;
        	        x -= dx[i];
        	        y -= dy[i];
        	    }

        	    // 结算
        	    if (count >= 5) {
        	        return true; 
        	    }
        	}

        	return false;
    	}

	public:
		// 初始化房间
		room(rid_t room_id, uid_t white_id, uid_t black_id, user_table* utp)
		: room_id_(room_id)
		, board_(BOARD_ROW, std::vector<int>(BOARD_COL, CHESS_EMPTY))
		, white_id_(white_id)
		, black_id_(black_id)
		, current_player_(black_id) // 黑子先下
		, stat_(start)
		, user_table_p_(utp)
		{
			ROOM_LOG(LOG_INFO, "初始化房间成功");	
		}

		// 销毁房间
		~room()
		{
			ROOM_LOG(LOG_INFO, "销毁房间成功");	
		}
		
		Json::Value handle_chess(const Json::Value& req)
		{
			oldking::lock_guard lock(mtx_);
               
			uid_t uid = req["uid"].asUInt64();
			int row = req["row"].asInt();
			int col = req["col"].asInt();

			Json::Value resp;   // 返回信息(返回到注册到websocket的回调函数?)
			// 公共字段
			// "room_id": room_id                   // 上级分配的room_id(或许)
            // "uid": (Json::UInt64)uid;            // 谁的请求

			// 落子专属字段
            // "result": "continue" / "gameover";   // 落子结果
            // "row": row;                          // 落子位置
            // "col": col;
			// "color": black / white               // 落子颜色 
            // "next_player": black_id / white_id;  // 下一个玩家
			// "winner": black_id / white_id;       // 胜者(没结束的话为空)
			// "info": how to win?;                 // 怎么赢的?
			
			// 聊天专属字段
			// "payload": payload                   // 聊天内容
			
			// 错误专属字段
			// "is error": true / false;            // 是否发生错误(可能会做修改使功能更复杂)

            // 落子入盘
            int color = (uid == black_id_) ? CHESS_BLACK : CHESS_WHITE;
            board_[row][col] = color;

            // 判断胜负
			bool is_win = check_win(row, col, uid);

			if (is_win) 
			{
                // 游戏结束处理
                stat_ = gameover;
			
				// 数据库更新 
				Json::Value winner;	
				Json::Value loser;
				winner["uid"] = uid;
				loser["uid"] = (uid == black_id_) ? white_id_ : black_id_;

				user_table_p_->win(winner);
				user_table_p_->win(loser);

                OL_MNG_LOG(LOG_INFO, "游戏结束! 胜者: " + std::to_string(uid));

                // 构建获胜的 JSON 广播给双方
            	
				// 公共字段
				// "room_id": room_id                   // 上级分配的room_id(或许)
            	// "uid": (Json::UInt64)uid;            // 谁的请求
				// "req_type": chess / exit / chat      // 区分请求类型

				// 落子专属字段
            	// "result": "continue" / "gameover";   // 落子结果
            	// "row": row;                          // 落子位置
            	// "col": col;
				// "color": black / white               // 落子颜色 
            	// "next_player": black_id / white_id;  // 下一个玩家
				// "winner": black_id / white_id;       // 胜者(没结束的话为空)
				// "info": how to win?;                 // 怎么赢的?
			
				// 聊天专属字段
				// "payload": payload                   // 聊天内容
				
				// 错误专属字段
				// "is error": true / false;            // 是否发生错误(可能会做修改使功能更复杂)
                

                resp["result"] = "gameover";
            	resp["uid"] = static_cast<Json::UInt64>(uid);         
                resp["row"] = row;
                resp["col"] = col;
				resp["color"] = uid == black_id_ ? "black" : "white";
                resp["winner"] = static_cast<Json::UInt64>(uid);
                resp["info"] = std::string() + "五子相连, 获胜者是" + (uid == black_id_ ? "黑方" : "白方");
            } 
			else 
			{
                // 游戏继续,切换行动方
                current_player_ = (uid == black_id_) ? white_id_ : black_id_;

                // 广播落子成功,通知前端更新棋盘,并轮到对方下棋
                Json::Value resp;
                resp["result"] = "continue";
            	resp["uid"] = static_cast<Json::UInt64>(uid);         
                resp["row"] = row;
                resp["col"] = col;
				resp["color"] = uid == black_id_ ? "black" : "white";
                resp["next_player"] = (Json::UInt64)current_player_;
            }
            
			return resp;
		}

		// 有一方跑路了调用这个	
		Json::Value handle_exit(const Json::Value& req)
		{
			oldking::lock_guard lock(mtx_);

			uid_t uid = req["uid"].asUInt64();

			// 更新数据
			Json::Value winner;	
			Json::Value loser;
			loser["uid"] = uid;
			winner["uid"] = (uid == black_id_) ? white_id_ : black_id_;

			user_table_p_->win(winner);
			user_table_p_->win(loser);
			
			// 广播获胜信息
			Json::Value resp;

            resp["result"] = "gameover";
            resp["winner"] = static_cast<Json::UInt64>(uid == black_id_ ? white_id_ : black_id_);
            resp["info"] = std::string() + "有人掉线了, 获胜者是" + (uid == black_id_ ? "白方" : "黑方");	
		
			return true;
		}

		// 聊天请求
		Json::Value handle_chat(const Json::Value& req)
		{
			oldking::lock_guard lock(mtx_);
			
			// 可以添加过滤啥的

			return req;
		}

		// 统一对请求做合法性分析和路由
		Json::Value handle_request(const Json::Value& req)
		{
			Json::Value resp;

			if(req["room_id"] != room_id_)
			{
				// req非法
            	ROOM_LOG(LOG_ERROR, "请求错误,房间号不正确");
				resp["is error"] = true;	
            	return resp; 
			}

			if(req["req_type"] == "chess")
			{
				// 检查uid, row, col 
            
				if(req["uid"] && req["row"] && req["col"])
				{
            	    ROOM_LOG(LOG_ERROR, "落子请求错误,不存在uid或row或col");
					resp["is error"] = true;	
            	    return resp; 
				}
			
				uid_t uid = req["uid"].asUInt64();
				int row = req["row"].asInt();
				int col = req["col"].asInt();

				if (stat_ == gameover) 
				{
            	    ROOM_LOG(LOG_WARNING, "落子请求错误,游戏已经结束");
					resp["is error"] = true;	
					return resp;
				}

            	if (uid != current_player_) 
				{
            	    ROOM_LOG(LOG_WARNING, "非法的落子请求: 未轮到玩家 " + std::to_string(uid) + " 或不存在该玩家");
					resp["is error"] = true;	
            	    return resp; 
            	}

            	if (row < 0 || row >= BOARD_ROW || col < 0 || col >= BOARD_COL || board_[row][col] != CHESS_EMPTY) 
				{
            	    ROOM_LOG(LOG_WARNING, "非法的落子坐标: (" + std::to_string(row) + "," + std::to_string(col) + ")");
					resp["is error"] = true;	
            	    return resp;
            	}
			
				return handle_chess(req);
			}
			else if (req["req_type"] == "exit")
			{
				// 检查uid 
				if(req["uid"])
				{
            	    ROOM_LOG(LOG_ERROR, "请求错误,不存在uid");
					resp["is error"] = true;	
            	    return resp; 
				}
			
				if(req["uid"].asUInt64() != black_id_ && req["uid"].asUInt64() != white_id_)
				{
            	    ROOM_LOG(LOG_ERROR, "请求错误uid不正确");
					resp["is error"] = true;	
            	    return resp; 
				}

				if (stat_ == gameover) 
				{
            	    ROOM_LOG(LOG_WARNING, "请求错误,游戏已经结束");
					resp["is error"] = true;	
					return resp;
				}

				return handle_exit(req);
			}
			else if (req["req_type"] == "chat")
			{
				// 检查payload, uid
				if(req["uid"] && req["payload"])
				{
            	    ROOM_LOG(LOG_ERROR, "聊天请求错误,不存在uid或payload");
					resp["is error"] = true;	
            	    return resp; 
				}
				
				if(req["uid"].asUInt64() != black_id_ && req["uid"].asUInt64() != white_id_)
				{
            	    ROOM_LOG(LOG_ERROR, "请求错误uid不正确");
					resp["is error"] = true;	
            	    return resp; 
				}
			
				if (stat_ == gameover) 
				{
            	    ROOM_LOG(LOG_WARNING, "聊天请求错误,游戏已经结束");
					resp["is error"] = true;	
					return resp;
				}

				return handle_chat(req);	
			}
			else 
			{
				// req非法
            	ROOM_LOG(LOG_ERROR, "请求错误,请求类型不正确");
				resp["is error"] = true;	
            	return resp; 
			}
		}

		// 属性获取函数
		rid_t get_room_id() { return room_id_; }

		uid_t get_white_id() { return white_id_; }
		uid_t get_black_id() { return black_id_; }
		room_stat get_room_stat() { return stat_; }

		// 因为room只能用于处理关于房间内的各种操作细节,而不是处理网络或者其他什么的东西,如果处理网络相关的内容,我觉得可能会有些权责问题
		// 为了更好的可测试性,整洁性,或者说更加优雅,我选择将所有的广播操作全部交给上级函数
	};
}


