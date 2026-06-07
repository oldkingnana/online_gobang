#pragma once

#include <memory>
#include <string>
#include <vector>
#include <jsoncpp/json/json.h>

#include "util.hpp"
#include "net_common.hpp"
#include "session_manager.hpp"
#include "session_detector.hpp"
#include "online_manager.hpp"
#include "matcher.hpp"
#include "room_manager.hpp"
#include "user_table.hpp"

namespace oldking
{
	// WebSocketServices 是 WebSocket 业务函数集合,它本身不保存连接状态,而是把具体业务组织成可注册到 WSRouter 的静态函数。
	// 它承担三层职责:
	// 1. 入口鉴权:从 WebSocket 握手请求中的 Cookie 取出 SSID,交给 session_detector 判断是否合法,合法后拿到 uid。
	// 2. 业务衔接:大厅业务负责进入大厅、开始/取消匹配、轮询检查房间;房间业务负责检查房间、落子、聊天、退出房间。
	// 3. 上层动作描述:业务函数不直接给其他连接发消息、不直接删除房间,而是在返回的 Json::Value 中附带 broadcast/cleanup,
	//    由 online_gobang_server::OnMessage 统一先回复当前客户端,再处理广播和状态清理。
	// 按你的设计思路,这里的函数都是“可注册业务逻辑”,但广播和断开清理这类强依赖 WebSocket 连接的动作只返回描述,交给 server 层执行。
	class WebSocketServices
	{
	private:
		// 从 Cookie 头字段中拆出 SSID,供 WebSocket 鉴权使用。
		static ssid_t get_ssid_from_cookie(const std::string& cookie)
		{
			std::vector<std::string> kvs;
			oldking::string_util::split(cookie, ";", kvs);

			for(auto& kv : kvs)
			{
				while(!kv.empty() && kv[0] == ' ')
				{
					kv.erase(0, 1);
				}

				std::vector<std::string> pair;
				oldking::string_util::split(kv, "=", pair, false);
				if(pair.size() == 2 && pair[0] == "SSID")
				{
					return pair[1];
				}
			}

			return "";
		}

		// 对 WebSocket 请求做统一鉴权:从连接请求头取 Cookie,解析 SSID,再通过 session_detector 换取 uid。
		static auth_result auth_ws(usr_conn_ptr_t conn, session_detector* sdp)
		{
			if(conn == nullptr)
			{
				auth_result result;
				result.ok = false;
				result.uid = 0;
				result.reason = "websocket connection is null";
				return result;
			}

			std::string cookie = conn->get_request().get_header("Cookie");
			ssid_t ssid = get_ssid_from_cookie(cookie);
			return sdp->check_ws(ssid);
		}

		// 根据鉴权失败结果构造当前业务类型的错误响应。
		static Json::Value make_auth_error_resp(const std::string& resp_type, const auth_result& auth)
		{
			return make_resp(resp_type, false, auth.reason);
		}

		// 构造用户没有房间状态时的统一错误响应。
		static Json::Value make_no_room_resp(const std::string& resp_type)
		{
			return make_resp(resp_type, false, "user is not in room");
		}

		// 构造 WebSocket 对前端的统一响应格式:resp_type/result/reason/data。
		static Json::Value make_resp(
				const std::string& resp_type,
				bool result,
				const std::string& reason,
				const Json::Value& data = Json::Value(Json::objectValue))
		{
			Json::Value resp;
			resp["resp_type"] = resp_type;
			resp["result"] = result;
			resp["reason"] = reason;
			resp["data"] = data;
			return resp;
		}

		// 从 room 对象中提取前端进入房间所需的完整房间信息,包括双方 uid、己方颜色、当前行动方和棋盘。
		static Json::Value make_room_info(const std::shared_ptr<room>& room_ptr, uid_t uid)
		{
			Json::Value data;
			data["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
			data["black_id"] = static_cast<Json::UInt64>(room_ptr->get_black_id());
			data["white_id"] = static_cast<Json::UInt64>(room_ptr->get_white_id());
			data["uid"] = static_cast<Json::UInt64>(uid);
			data["color"] = (uid == room_ptr->get_black_id()) ? "black" : "white";
			data["current_player"] = static_cast<Json::UInt64>(room_ptr->get_current_player());
			data["room_stat"] = (room_ptr->get_room_stat() == start) ? "start" : "gameover";
			data["board"] = room_ptr->get_board();
			return data;
		}

		// 把 room 内部返回格式包成 WebSocket 统一响应格式,将 room 的 is error 转换成 WS 层 result。
		static Json::Value wrap_room_resp(const std::string& resp_type, Json::Value& room_resp)
		{
			bool ok = !room_resp["is error"].asBool();
			std::string reason = ok ? "room request success" : "room request failed";
			return make_resp(resp_type, ok, reason, room_resp);
		}

		// 根据当前 uid 和房间黑白双方 uid,计算对手 uid。
		static uid_t get_peer_uid(const std::shared_ptr<room>& room_ptr, uid_t uid)
		{
			return (uid == room_ptr->get_black_id()) ? room_ptr->get_white_id() : room_ptr->get_black_id();
		}

		// 在当前响应中附加“发给对手”的广播描述,由 server 层根据 target_uid 找连接并发送 data。
		static void add_peer_broadcast(Json::Value& resp, uid_t peer_uid, const Json::Value& data)
		{
			resp["broadcast"]["target"] = "uid";
			resp["broadcast"]["target_uid"] = static_cast<Json::UInt64>(peer_uid);
			resp["broadcast"]["data"] = data;
		}

		// 在当前响应中附加“删除房间并清理双方在线房间状态”的内部清理描述。
		static void add_room_cleanup(Json::Value& resp, const std::shared_ptr<room>& room_ptr)
		{
			resp["cleanup"]["type"] = "delete_room";
			resp["cleanup"]["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
			resp["cleanup"]["black_id"] = static_cast<Json::UInt64>(room_ptr->get_black_id());
			resp["cleanup"]["white_id"] = static_cast<Json::UInt64>(room_ptr->get_white_id());
		}

	public:
		WebSocketServices() = delete;
		~WebSocketServices() = delete;

		// 大厅进入业务:鉴权成功后把 uid 和当前 WebSocket 连接登记到 online_manager 的大厅状态中。
		static Json::Value s_enter_hall(
				session_detector* sdp,
				online_manager* omp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)req;

			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp("enter_hall", auth);
			}

			omp->enter_hall(auth.uid, conn);

			return make_resp("enter_hall", true, "enter hall success");
		}

		// 开始匹配业务:鉴权成功后把用户放入 matcher 对应积分队列,等待匹配线程创建房间。
		static Json::Value s_match_start(
				session_detector* sdp,
				matcher* mp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)req;

			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp("match_start", auth);
			}

			if(mp->push(auth.uid) == false)
			{
				return make_resp("match_start", false, "user is not in hall");
			}

			return make_resp("match_start", true, "match start success");
		}

		// 取消匹配业务:鉴权成功后从 matcher 队列中移除当前用户。
		static Json::Value s_match_cancel(
				session_detector* sdp,
				matcher* mp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)req;

			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp("match_cancel", auth);
			}

			mp->rm(auth.uid);

			return make_resp("match_cancel", true, "match cancel success");
		}

		// 离开大厅业务:鉴权成功后取消匹配并清理 online_manager 中的大厅连接状态。
		static Json::Value s_leave_hall(
				session_detector* sdp,
				online_manager* omp,
				matcher* mp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)req;

			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp("leave_hall", auth);
			}

			mp->rm(auth.uid);
			omp->exit_hall(auth.uid);

			return make_resp("leave_hall", true, "leave hall success");
		}

		// 检查房间业务:大厅轮询时只探测是否已经匹配成功;房间页进入时把用户从大厅状态切换到房间状态。
		static Json::Value s_check_room(
				session_detector* sdp,
				online_manager* omp,
				room_manager* rmp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)req;

			const std::string resp_type = "check_room";
			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp(resp_type, auth);
			}

			auto room_ptr = rmp->get_room_by_uid(auth.uid);
			if(room_ptr == nullptr)
			{
				return make_no_room_resp(resp_type);
			}

			if(!req["probe"].asBool())
			{
				omp->exit_hall(auth.uid);
				omp->enter_room(auth.uid, conn);
			}

			return make_resp(resp_type, true, "room found", make_room_info(room_ptr, auth.uid));
		}

		// 退出房间业务:鉴权后调用 room 的 exit 逻辑结算胜负,再返回对手广播和房间清理描述。
		static Json::Value s_leave_room(
				session_detector* sdp,
				online_manager* omp,
				room_manager* rmp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;
			(void)omp;

			const std::string resp_type = "leave_room";
			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp(resp_type, auth);
			}

			auto room_ptr = rmp->get_room_by_uid(auth.uid);
			if(room_ptr == nullptr)
			{
				return make_no_room_resp(resp_type);
			}

			Json::Value room_req = req;
			room_req["uid"] = static_cast<Json::UInt64>(auth.uid);
			room_req["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
			room_req["req_type"] = "exit";

			Json::Value resp = room_ptr->handle_request(room_req);
			resp["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());

			Json::Value ws_resp = wrap_room_resp(resp_type, resp);
			if(ws_resp["result"].asBool())
			{
				uid_t peer_uid = get_peer_uid(room_ptr, auth.uid);
				add_peer_broadcast(ws_resp, peer_uid, make_resp("peer_leave_room", true, "peer leave room", resp));
				add_room_cleanup(ws_resp, room_ptr);
			}
			return ws_resp;
		}

		// 落子业务:鉴权后把客户端 row/col 补齐 uid/room_id/内部 req_type,交给 room 执行落子规则和胜负判断。
		// 落子成功会广播给对手;如果本次落子导致 gameover,还会附加 cleanup 让 server 删除房间状态。
		static Json::Value s_put_chess(
				session_detector* sdp,
				room_manager* rmp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;

			const std::string resp_type = "put_chess";
			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp(resp_type, auth);
			}

			auto room_ptr = rmp->get_room_by_uid(auth.uid);
			if(room_ptr == nullptr)
			{
				return make_no_room_resp(resp_type);
			}

			Json::Value room_req = req;
			room_req["uid"] = static_cast<Json::UInt64>(auth.uid);
			room_req["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
			room_req["req_type"] = "chess";

			Json::Value resp = room_ptr->handle_request(room_req);
			resp["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());

			Json::Value ws_resp = wrap_room_resp(resp_type, resp);
			if(ws_resp["result"].asBool())
			{
				uid_t peer_uid = get_peer_uid(room_ptr, auth.uid);
				add_peer_broadcast(ws_resp, peer_uid, make_resp("peer_put_chess", true, "peer put chess", resp));
				if(resp["result"].asString() == "gameover")
				{
					add_room_cleanup(ws_resp, room_ptr);
				}
			}
			return ws_resp;
		}

		// 房间聊天业务:鉴权后确认用户所在房间,把聊天内容交给 room 做基本校验,成功后广播给对手。
		static Json::Value s_room_chat(
				session_detector* sdp,
				room_manager* rmp,
				usr_conn_ptr_t conn,
				msg_ptr_t msg,
				const Json::Value& req)
		{
			(void)msg;

			const std::string resp_type = "room_chat";
			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return make_auth_error_resp(resp_type, auth);
			}

			auto room_ptr = rmp->get_room_by_uid(auth.uid);
			if(room_ptr == nullptr)
			{
				return make_no_room_resp(resp_type);
			}

			Json::Value room_req = req;
			room_req["uid"] = static_cast<Json::UInt64>(auth.uid);
			room_req["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
			room_req["req_type"] = "chat";

			Json::Value resp = room_ptr->handle_request(room_req);
			resp["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());

			Json::Value ws_resp = wrap_room_resp(resp_type, resp);
			if(ws_resp["result"].asBool())
			{
				uid_t peer_uid = get_peer_uid(room_ptr, auth.uid);
				add_peer_broadcast(ws_resp, peer_uid, make_resp("peer_room_chat", true, "peer room chat", resp));
			}
			return ws_resp;
		}

		// WebSocket 断开清理业务:连接关闭时根据 uid 所在状态清理大厅/匹配/房间。
		// 如果用户正在房间内且游戏未结束,调用 room 的 exit 逻辑给对手构造获胜通知,再附加房间 cleanup。
		static Json::Value s_on_close(
				session_detector* sdp,
				online_manager* omp,
				matcher* mp,
				room_manager* rmp,
				usr_conn_ptr_t conn)
		{
			Json::Value resp = make_resp("connection_close", true, "connection closed");
			auth_result auth = auth_ws(conn, sdp);
			if(!auth.ok)
			{
				return resp;
			}

			mp->rm(auth.uid);

			if(omp->in_hall(auth.uid))
			{
				omp->exit_hall(auth.uid);
			}

			if(!omp->in_room(auth.uid))
			{
				return resp;
			}

			auto room_ptr = rmp->get_room_by_uid(auth.uid);
			if(room_ptr == nullptr)
			{
				omp->exit_room(auth.uid);
				return resp;
			}

			if(room_ptr->get_room_stat() == start)
			{
				Json::Value room_req;
				room_req["uid"] = static_cast<Json::UInt64>(auth.uid);
				room_req["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
				room_req["req_type"] = "exit";
				Json::Value room_resp = room_ptr->handle_request(room_req);
				room_resp["room_id"] = static_cast<Json::UInt64>(room_ptr->get_room_id());
				uid_t peer_uid = get_peer_uid(room_ptr, auth.uid);
				add_peer_broadcast(resp, peer_uid, make_resp("peer_leave_room", true, "peer disconnected", room_resp));
			}

			add_room_cleanup(resp, room_ptr);
			return resp;
		}
	};
}
