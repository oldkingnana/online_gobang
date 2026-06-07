#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

namespace oldking
{
	// websocketpp 服务端类型别名,统一项目中 HTTP/WebSocket 相关类型的写法。
	typedef websocketpp::server<websocketpp::config::asio> ws_server_t;
	// WebSocket 消息指针类型,OnMessage 和 WSRouter 之间传递客户端消息时使用。
	typedef ws_server_t::message_ptr msg_ptr_t;
	// WebSocket/HTTP 连接对象指针类型,用于读取请求头、发送消息和设置 HTTP 响应。
	typedef ws_server_t::connection_ptr usr_conn_ptr_t;
	// websocketpp 连接句柄类型,server 回调收到句柄后再换成 connection_ptr。
	typedef websocketpp::connection_hdl conn_hdl_t;
	// websocketpp HTTP 请求类型别名,供 HttpRouter 和 HTTPServices 读取请求数据。
	typedef websocketpp::http::parser::request http_req_t;
	// websocketpp HTTP 响应类型别名,供 HTTPServices 和 HttpRouter 构造响应。
	typedef websocketpp::http::parser::response http_resp_t;
}
