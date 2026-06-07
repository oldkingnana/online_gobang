#pragma once

#include <functional>
#include <map>
#include <string>
#include <jsoncpp/json/json.h>

#include "util.hpp"
#include "net_common.hpp"
#include "mutex.hpp"
#include "myeasylog.hpp"

#ifndef WS_ROUTER_LOG
#define WS_ROUTER_LOG(level, msg) \
    do { \
        std::stringstream ss; \
        ss << msg; \
        oldking::MyEasyLog::GetInstance().WriteLog(level, __FILE__, ss.str()); \
    } while (0)
#endif

namespace oldking
{
        // WebSocket messages are routed by req_type.
        // WSRouterKey 描述一个 WebSocket 业务入口,目前只用 req_type 区分业务。
        // 前端消息必须携带 req_type,WSRouter 才能找到对应注册函数。
        struct WSRouterKey
        {
                WSRouterKey() = default;
                ~WSRouterKey() = default;

                // 判断两个 WebSocket 路由键是否代表同一个 req_type。
                bool operator==(const WSRouterKey& other) const
                {
                        return req_type == other.req_type;
                }

                // 为 std::map 提供按 req_type 排序的比较规则。
                bool operator<(const WSRouterKey& other) const
                {
                        return req_type < other.req_type;
                }

                std::string req_type;
        };

        // WSRouterKey 的比较器,用于维护 req_type 到回调函数的有序映射。
        class ws_less
        {
        public:
                // 调用 WSRouterKey 自身的 operator< 完成路由键比较。
                bool operator()(const WSRouterKey& k1, const WSRouterKey& k2) const
                {
                        return k1 < k2;
                }
        };

        // WSRouter 是 WebSocket 动态路由器,负责把前端发来的 JSON 文本解析成 req_type,再分发到对应业务函数。
        // 它完全按动态注册工作:大厅、匹配、房间等业务都通过 addRouter 注入,路由器自身不关心业务细节。
        // 和 HTTP 的 OnHttp 类似,它只负责“拿请求、找回调、拿响应”;真正发送响应由 online_gobang_server::OnMessage 处理。
        // 这样可以让 WSRouter 高内聚、低耦合,业务函数也统一返回 Json::Value,便于上层统一发送、广播和清理。
        class WSRouter
        {
        private:
                typedef std::function<Json::Value(usr_conn_ptr_t, msg_ptr_t, const Json::Value&)> ws_func_t;

                std::map<WSRouterKey, ws_func_t, ws_less> func_map_;
                oldking::mymutex mtx_;

                // 构造路由错误响应,用于 JSON 解析失败、缺少 req_type 或未找到注册函数等情况。
                Json::Value make_error_resp(const std::string& reason)
                {
                        Json::Value resp;
                        resp["resp_type"] = "router_error";
                        resp["result"] = false;
                        resp["reason"] = reason;
                        resp["data"] = Json::Value(Json::objectValue);
                        return resp;
                }

        public:
                WSRouter()
                : func_map_({})
                {}

                WSRouter(const WSRouter& other)
                : func_map_(other.func_map_)
                {}

                ~WSRouter()
                {}

                // 注册一个 WebSocket 业务函数,把 req_type 绑定到对应回调。
                void addRouter(
                                const WSRouterKey& key,
                                ws_func_t func
                                )
                {
                        oldking::lock_guard lock(mtx_);
                        func_map_.insert({key, func});
                        return;
                }

                // 解析 WebSocket 消息并执行动态路由,最终把业务回调返回的 Json::Value 交还给上层发送。
                Json::Value routing(usr_conn_ptr_t conn, msg_ptr_t msg)
                {
                        WS_ROUTER_LOG(LOG_INFO, "received websocket message");

                        if(conn == nullptr)
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket connection is null");
                                return make_error_resp("websocket connection is null");
                        }

                        if(msg == nullptr)
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message is null");
                                return make_error_resp("websocket message is null");
                        }

                        Json::Value req;
                        if(oldking::json_util::deserialize(msg->get_payload(), req) == false)
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message is not valid json");
                                return make_error_resp("websocket message is not valid json");
                        }

                        if(!req["req_type"])
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message has no req_type");
                                return make_error_resp("websocket message has no req_type");
                        }

                        WSRouterKey key;
                        key.req_type = req["req_type"].asString();

                        ws_func_t func;
                        {
                                oldking::lock_guard lock(mtx_);
                                if(func_map_.find(key) == func_map_.end())
                                {
                                        WS_ROUTER_LOG(LOG_ERROR, "websocket route does not exist: " + key.req_type);
                                        return make_error_resp("websocket route does not exist");
                                }

                                func = func_map_[key];
                        }

                        return func(conn, msg, req);
                }
        };
}
