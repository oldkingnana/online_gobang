#pragma once 

#include <functional>
#include <memory>
#include <map>

#include "util.hpp"
#include "common.hpp"
#include "net_common.hpp"
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
		// RouterKey 描述一个 HTTP 路由入口,method 用于区分 GET/POST,path 用于区分访问路径。
		// 它作为 std::map 的键,把一个 HTTP 请求准确映射到注册好的业务函数。
        struct RouterKey
        {
                RouterKey() = default;
                ~RouterKey() = default;

				// 判断两个 HTTP 路由键是否指向同一个 method/path。
                bool operator==(const RouterKey& other) const 
                {
                        return method == other.method && path == other.path;
                }

				// 为 std::map 提供有序比较规则,先比较 method,再比较 path。
				bool operator<(const RouterKey& other) const 
				{
				    return std::tie(method, path) < std::tie(other.method, other.path);
				}

                std::string method;
                std::string path;
        };

		// RouterKey 的比较器,用于让动态路由表可以按 method/path 维护有序映射。
        class less 
        {
        public:
				// 调用 RouterKey 自身的 operator< 完成路由键比较。
                bool operator()(const RouterKey& k1, const RouterKey& k2) const 
                {
                        return k1 < k2;
                }
        };

        // 业务分发
		// HttpRouter 是 HTTP 层的分发器,它同时负责动态业务路由和静态资源路由。
		// 动态路由部分由外部通过 addRouter 注册,例如 /login、/register 这类需要执行业务代码的请求。
		// 静态路由部分在没有命中动态路由时触发,从 wwwroot 中读取 html/js/png 等文件并设置对应 MIME。
		// 这个类只负责“找到应该处理请求的人”,不直接理解登录、注册、房间等具体业务含义。
        class HttpRouter
        {
        private:
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

				// 注册一条 HTTP 动态路由,把 method/path 绑定到一个业务处理函数。
                void addRouter(
                                const RouterKey& key,  
                                std::function<http_resp_t(const http_req_t&)> func
                                )
				{
						oldking::lock_guard lock(mtx_);
				        func_map_.insert({key, func});
				        return;
				}

				// 根据请求 method/path 执行路由分发;优先调用动态业务函数,未命中时尝试读取静态资源,最后返回 404。
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
