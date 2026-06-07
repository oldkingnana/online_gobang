#pragma once

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

namespace oldking
{
	typedef websocketpp::server<websocketpp::config::asio> ws_server_t;
	typedef ws_server_t::message_ptr msg_ptr_t;
	typedef ws_server_t::connection_ptr usr_conn_ptr_t;
	typedef websocketpp::connection_hdl conn_hdl_t;
	typedef websocketpp::http::parser::request http_req_t;
	typedef websocketpp::http::parser::response http_resp_t;
}
