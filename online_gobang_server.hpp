#pragma once 

#include "myeasylog.hpp"
#include "online_manager.hpp"
#include "room_manager.hpp"
#include "session_manager.hpp"
#include "user_table.hpp"
#include "matcher.hpp"
#include "util.hpp"
#include "HTTPRouter.hpp"

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
	class online_gobang_server
	{
	private:
		typedef websocketpp::http::parser::request http_req_t;
		typedef websocketpp::http::parser::response http_resp_t;
		typedef websocketpp::server<websocketpp::config::asio> ws_server;
		typedef ws_server::message_ptr msg_ptr; 
		
		ws_server server_;
		oldking::HttpRouter http_router_; // 一定要写一个接口用于给router注册业务代码	

		// 注册函数
		void OnHttp(websocketpp::connection_hdl hdl) 
		{
			// 请求接收
			ws_server::connection_ptr pcon = server_.get_con_from_hdl(hdl);
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

	public:
		void http_add_router(
				const RouterKey& key, 
				std::function<http_resp_t(const http_req_t&)> func
				)
		{
			http_router_.addRouter(key, func);
		}

		void start()
		{
			// 日志设置
			server_.set_access_channels(websocketpp::log::alevel::none);

			// 初始化异步输入输出
			server_.init_asio();

			// 注册http请求回调函数(太优雅了!)
			server_.set_http_handler([this](websocketpp::connection_hdl hdl) {
				this->OnHttp(hdl);	
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






