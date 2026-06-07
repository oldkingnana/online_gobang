#pragma once 

#include <functional>

#include "myeasylog.hpp"
#include "online_manager.hpp"
#include "room_manager.hpp"
#include "session_manager.hpp"
#include "user_table.hpp"
#include "matcher.hpp"
#include "util.hpp"
#include "HTTPRouter.hpp"
#include "WSRouter.hpp"
#include "net_common.hpp"
#include <websocketpp/frame.hpp>

#ifndef SERVER_LOG
#define SERVER_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking
{
	// online_gobang_server 是整个服务端的网络入口和调度外壳,负责把 websocketpp 的底层回调接入项目自己的路由系统。
	// 它内部持有两个路由器:HttpRouter 处理 HTTP 请求和静态资源,WSRouter 处理 WebSocket 消息中的 req_type。
	// 它不直接实现登录、匹配、落子等具体业务,而是提供 http_add_router/ws_add_router 让 main.cpp 注入业务函数。
	// 对 WebSocket 来说,它还承担三个强连接相关的特殊动作:
	// 1. close handler:连接断开时调用业务侧清理逻辑;
	// 2. broadcast handler:根据业务返回的 broadcast 描述找到目标连接并发送消息;
	// 3. cleanup handler:根据业务返回的 cleanup 描述清理 online_manager/room_manager 状态。
	// 这样业务层只需要返回 Json::Value,server 层统一负责发送、广播和状态尾处理。
	class online_gobang_server
	{
	private:
		ws_server_t server_;
		oldking::WSRouter ws_router_;
		oldking::HttpRouter http_router_; // 一定要写一个接口用于给router注册业务代码	
		std::function<Json::Value(usr_conn_ptr_t)> ws_close_func_;
		std::function<usr_conn_ptr_t(const Json::Value&)> ws_broadcast_func_;
		std::function<void(const Json::Value&)> ws_cleanup_func_;

		// 处理 HTTP 请求:交给 HttpRouter 路由,再把业务或静态资源响应写回 websocketpp 连接。
		void OnHttp(conn_hdl_t hdl) 
		{
			// 请求接收
			usr_conn_ptr_t pcon = server_.get_con_from_hdl(hdl);
			http_req_t req = pcon->get_request();

			// 请求路由 + 请求处理
			
			http_resp_t resp = http_router_.routing(req);	

			// 结果回复
			const auto& headers = resp.get_headers();

			// log 
			SERVER_LOG(LOG_INFO, "本次回复内容");	
			SERVER_LOG(LOG_INFO, std::string() + "stat: \n" + std::to_string(resp.get_status_code()));	
			SERVER_LOG(LOG_INFO, "body: \n" + resp.get_body());	
			SERVER_LOG(LOG_INFO, "headers:  \n");	
    		for (const auto& header : headers) {
        		// 注意：websocketpp 可能会自带一些基础 header，所以用 append_header 最安全
				SERVER_LOG(LOG_INFO, header.first + header.second);	
    		}
	
			pcon->set_status(resp.get_status_code());
			pcon->set_body(resp.get_body());

    		for (const auto& header : headers) {
        		// 注意：websocketpp 可能会自带一些基础 header，所以用 append_header 最安全
        		pcon->append_header(header.first, header.second);
    		}

			//std::stringstream ss;
			//ss << "<!doctype html><html><head>" 
			//   << "<title>hello websocket</title><body>"
			//   << "<h1>hello websocketpp</h1>"
			//   << "</body></head></html>";

			//pcon->set_body(ss.str());
			//pcon->set_status(websocketpp::http::status_code::ok);
		}

		// 处理 WebSocket 普通消息:交给 WSRouter 获取响应,先回复当前客户端,再处理广播和清理等内部动作。
		void OnMessage(conn_hdl_t hdl, msg_ptr_t msg)
		{
			usr_conn_ptr_t pcon = server_.get_con_from_hdl(hdl);
			Json::Value resp = ws_router_.routing(pcon, msg);

			Json::Value client_resp = resp;
			client_resp.removeMember("broadcast");
			client_resp.removeMember("cleanup");
			send_json(pcon, client_resp);
			handle_inner_actions(resp);
		}

		// 处理 WebSocket 连接关闭:调用外部注册的 close handler,并继续执行其返回的广播/清理动作。
		void OnClose(conn_hdl_t hdl)
		{
			usr_conn_ptr_t pcon = server_.get_con_from_hdl(hdl);
			if(ws_close_func_)
			{
				Json::Value resp = ws_close_func_(pcon);
				handle_inner_actions(resp);
			}
		}

		// 将 Json::Value 序列化成文本并通过 WebSocket 发送给指定连接。
		void send_json(usr_conn_ptr_t conn, const Json::Value& resp)
		{
			if(conn == nullptr)
			{
				return;
			}

			std::string body;
			oldking::json_util::serialize(resp, body);
			conn->send(body, websocketpp::frame::opcode::text);
		}

		// 执行业务响应中的内部动作:先根据 broadcast 给目标连接发消息,再根据 cleanup 清理房间和在线状态。
		void handle_inner_actions(Json::Value& resp)
		{
			Json::Value broadcast = resp["broadcast"];
			Json::Value cleanup = resp["cleanup"];
			resp.removeMember("broadcast");
			resp.removeMember("cleanup");

			if(!broadcast.isNull() && ws_broadcast_func_)
			{
				usr_conn_ptr_t target_conn = ws_broadcast_func_(broadcast);
				if(target_conn != nullptr)
				{
					send_json(target_conn, broadcast["data"]);
				}
			}

			if(!cleanup.isNull() && ws_cleanup_func_)
			{
				ws_cleanup_func_(cleanup);
			}
		}

	public:
		// 注册 HTTP 动态路由,把 method/path 对应到具体业务函数。
		void http_add_router(
				const RouterKey& key, 
				std::function<http_resp_t(const http_req_t&)> func
				)
		{
			http_router_.addRouter(key, func);
		}

		// 注册 WebSocket 动态路由,把 req_type 对应到具体业务函数。
		void ws_add_router(
				const WSRouterKey& key,
				std::function<Json::Value(usr_conn_ptr_t, msg_ptr_t, const Json::Value&)> func
				)
		{
			ws_router_.addRouter(key, func);
		}

		// 注册 WebSocket 断开连接时的业务清理函数。
		void ws_set_close_handler(std::function<Json::Value(usr_conn_ptr_t)> func)
		{
			ws_close_func_ = func;
		}

		// 注册广播目标查找函数,server 层通过它把 broadcast 描述转换成实际连接。
		void ws_set_broadcast_handler(std::function<usr_conn_ptr_t(const Json::Value&)> func)
		{
			ws_broadcast_func_ = func;
		}

		// 注册内部状态清理函数,server 层通过它执行房间删除和在线状态清除。
		void ws_set_cleanup_handler(std::function<void(const Json::Value&)> func)
		{
			ws_cleanup_func_ = func;
		}

		// 初始化 websocketpp 服务端,绑定 HTTP/WS/Close 回调,监听端口并进入事件循环。
		void start()
		{
			// 日志设置
			server_.set_access_channels(websocketpp::log::alevel::none);

			// 初始化异步输入输出
			server_.init_asio();

			// 注册http请求回调函数(太优雅了!)
			server_.set_http_handler([this](conn_hdl_t hdl) {
				this->OnHttp(hdl);	
			});

			server_.set_message_handler([this](conn_hdl_t hdl, msg_ptr_t msg) {
				this->OnMessage(hdl, msg);
			});

			server_.set_close_handler([this](conn_hdl_t hdl) {
				this->OnClose(hdl);
			});
					
			// 监听端口
			server_.listen(7777);

			// 启动事件处理
			server_.start_accept();

			// 启动服务器
			server_.run();
		}
	};
}






