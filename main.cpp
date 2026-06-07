#include <memory>

#include "online_gobang_server.hpp"
#include "HTTPServices.hpp"
#include "WebSocketServices.hpp"

// 程序入口:创建所有核心组件,注册 HTTP/WS 路由和特殊回调,最后启动在线五子棋服务端。
int main()
{
	// 创建全局业务组件:session 管理、用户表访问、在线状态、房间管理、匹配器和鉴权过滤器。
	auto session_manager = std::make_shared<oldking::session_manager>();
	oldking::user_table user_table("127.0.0.1", "root", "", "Online_Gobang");
	oldking::online_manager online_manager;
	oldking::room_manager room_manager(&online_manager, &user_table);
	oldking::matcher matcher(&user_table, &online_manager, &room_manager);
	oldking::session_detector session_detector(session_manager);
	oldking::online_gobang_server server;

	// 注册 HTTP 登录路由,前端 POST /login 时会调用 HTTPServices::s_login。
	oldking::RouterKey login_key;
	login_key.method = "POST";
	login_key.path = "/login";
	server.http_add_router(login_key, [session_manager, &user_table](const auto& req) {
		return oldking::HTTPServices::s_login(session_manager.get(), &user_table, req);
	});

	// 注册 HTTP 注册路由,前端 POST /register 时会调用 HTTPServices::s_register。
	oldking::RouterKey register_key;
	register_key.method = "POST";
	register_key.path = "/register";
	server.http_add_router(register_key, [session_manager, &user_table](const auto& req) {
		return oldking::HTTPServices::s_register(session_manager.get(), &user_table, req);
	});

	// 注册进入大厅业务,用于用户打开 hall.html 后建立大厅在线状态。
	oldking::WSRouterKey enter_hall_key;
	enter_hall_key.req_type = "enter_hall";
	server.ws_add_router(enter_hall_key, [&session_detector, &online_manager](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_enter_hall(&session_detector, &online_manager, conn, msg, req);
	});

	// 注册开始匹配业务,用于把大厅用户放入 matcher 队列。
	oldking::WSRouterKey match_start_key;
	match_start_key.req_type = "match_start";
	server.ws_add_router(match_start_key, [&session_detector, &matcher](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_match_start(&session_detector, &matcher, conn, msg, req);
	});

	// 注册取消匹配业务,用于从 matcher 队列中移除当前用户。
	oldking::WSRouterKey match_cancel_key;
	match_cancel_key.req_type = "match_cancel";
	server.ws_add_router(match_cancel_key, [&session_detector, &matcher](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_match_cancel(&session_detector, &matcher, conn, msg, req);
	});

	// 注册离开大厅业务,用于清理大厅在线状态和匹配队列状态。
	oldking::WSRouterKey leave_hall_key;
	leave_hall_key.req_type = "leave_hall";
	server.ws_add_router(leave_hall_key, [&session_detector, &online_manager, &matcher](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_leave_hall(&session_detector, &online_manager, &matcher, conn, msg, req);
	});

	// 注册检查房间业务,大厅轮询用它判断是否匹配成功,房间页进入时用它绑定房间在线状态。
	oldking::WSRouterKey check_room_key;
	check_room_key.req_type = "check_room";
	server.ws_add_router(check_room_key, [&session_detector, &online_manager, &room_manager](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_check_room(&session_detector, &online_manager, &room_manager, conn, msg, req);
	});

	// 注册退出房间业务,用于主动离开房间并触发胜负结算、广播和房间清理。
	oldking::WSRouterKey leave_room_key;
	leave_room_key.req_type = "leave_room";
	server.ws_add_router(leave_room_key, [&session_detector, &online_manager, &room_manager](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_leave_room(&session_detector, &online_manager, &room_manager, conn, msg, req);
	});

	// 注册落子业务,用于把前端 row/col 请求交给 room 执行规则判断。
	oldking::WSRouterKey put_chess_key;
	put_chess_key.req_type = "put_chess";
	server.ws_add_router(put_chess_key, [&session_detector, &room_manager](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_put_chess(&session_detector, &room_manager, conn, msg, req);
	});

	// 注册房间聊天业务,用于把房间内聊天消息广播给对手。
	oldking::WSRouterKey room_chat_key;
	room_chat_key.req_type = "room_chat";
	server.ws_add_router(room_chat_key, [&session_detector, &room_manager](auto conn, auto msg, const auto& req) {
		return oldking::WebSocketServices::s_room_chat(&session_detector, &room_manager, conn, msg, req);
	});

	// 注册 WebSocket 断开处理函数,用于连接关闭后清理匹配、大厅、房间状态并通知对手。
	server.ws_set_close_handler([&session_detector, &online_manager, &matcher, &room_manager](auto conn) {
		return oldking::WebSocketServices::s_on_close(&session_detector, &online_manager, &matcher, &room_manager, conn);
	});

	// 注册广播目标查找函数,当前只支持按 uid 从房间在线表中查找对手连接。
	server.ws_set_broadcast_handler([&online_manager](const Json::Value& broadcast) {
		if(broadcast["target"].asString() != "uid" || broadcast["target_uid"].isNull())
		{
			return oldking::usr_conn_ptr_t();
		}

		oldking::uid_t target_uid = broadcast["target_uid"].asUInt64();
		return online_manager.find_from_room(target_uid);
	});

	// 注册内部清理函数,当前 cleanup 主要用于删除房间并移除双方房间在线状态。
	server.ws_set_cleanup_handler([&online_manager, &room_manager](const Json::Value& cleanup) {
		if(cleanup["type"].asString() != "delete_room")
		{
			return;
		}

		oldking::uid_t black_id = cleanup["black_id"].asUInt64();
		oldking::uid_t white_id = cleanup["white_id"].asUInt64();
		oldking::rid_t room_id = cleanup["room_id"].asUInt64();

		online_manager.exit_room(black_id);
		online_manager.exit_room(white_id);
		room_manager.delete_room(room_id);
	});

	server.start();

	return 0;
}


