#pragma once 

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <functional>
#include <memory>
#include <map>

#include "util.hpp"
#include "common.hpp"
#include "mutex.hpp"
#include "myeasylog.hpp"

#ifndef ROUTER_LOG
#define ROUTER_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking 
{
		#define WEBROOT "./wwwroot"

		// 对于HTTP来说,我们用method和path区分业务,所以搞一个RouterKey用于业务类别和注册函数的映射
        struct RouterKey
        {
                RouterKey() = default;
                ~RouterKey() = default;

                bool operator==(const RouterKey& other) const 
                {
                        return method == other.method && path == other.path;
                }

				bool operator<(const RouterKey& other) const 
				{
				    return std::tie(method, path) < std::tie(other.method, other.path);
				}

                std::string method;
                std::string path;
        };

        class less 
        {
        public:
                bool operator()(const RouterKey& k1, const RouterKey& k2) const 
                {
                        return k1 < k2;
                }
        };

        // 业务分发
        class HttpRouter
        {
        private:
				typedef websocketpp::server<websocketpp::config::asio> ws_server;
				typedef ws_server::message_ptr msg_ptr; 
				typedef websocketpp::http::parser::request http_req_t;
				typedef websocketpp::http::parser::response http_resp_t;
                
				std::map<RouterKey, std::function<http_resp_t(const http_req_t&)>, less>func_map_;
				oldking::mymutex mtx_;

        public:
                HttpRouter()
                : func_map_({})
                {}

                HttpRouter(const HttpRouter& other)
                : func_map_(other.func_map_)
                {}

                ~HttpRouter()
                {}

                void addRouter(
                                const RouterKey& key,  
                                std::function<http_resp_t(const http_req_t&)> func
                                )
				{
						oldking::lock_guard lock(mtx_);
				        func_map_.insert({key, func});
				        return;
				}

                http_resp_t routing(const http_req_t& req)
				{
						ROUTER_LOG(LOG_INFO, "接收到了请求");		
					
				        RouterKey k;
				        k.method = req.get_method();
				        k.path = req.get_uri();
				
				        // 动态路由
				        if(func_map_.find(k) != func_map_.end())
				        {
				                return func_map_[k](req);
				        }
				
				        // 静态路由
				        http_resp_t res;
				        std::string exten;

						std::string html;
				        if(req.get_uri() == "/" && oldking::file_util::read(WEBROOT + std::string("/index.html"), html))
				        {
								res.set_body(html);
								res.set_status(websocketpp::http::status_code::value::ok);
				                auto header = res.get_headers();
								res.append_header("Content-Type", "text/html; charset=UTF-8");
				        }
				        else if(oldking::file_util::GetFileExten(req.get_uri(), exten) && oldking::file_util::read(WEBROOT + req.get_uri(), html))
				        {
								res.set_body(html);
								res.set_status(websocketpp::http::status_code::value::ok);
								res.append_header("Content-Type", mime_map.find(exten)->second + "; charset=UTF-8");
				        }
        				else 
        				{
        				        // 404 
        				        oldking::file_util::read(WEBROOT + std::string("/404.html"), html);
								res.set_body(html);
								res.set_status(websocketpp::http::status_code::value::not_found);
								res.append_header("Content-Type", "text/html; charset=UTF-8");
        				}
						ROUTER_LOG(LOG_INFO, "发送了回复");		
 
        				return res;
				}
        };
}
