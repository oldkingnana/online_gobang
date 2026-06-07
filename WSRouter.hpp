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
        struct WSRouterKey
        {
                WSRouterKey() = default;
                ~WSRouterKey() = default;

                bool operator==(const WSRouterKey& other) const
                {
                        return req_type == other.req_type;
                }

                bool operator<(const WSRouterKey& other) const
                {
                        return req_type < other.req_type;
                }

                std::string req_type;
        };

        class ws_less
        {
        public:
                bool operator()(const WSRouterKey& k1, const WSRouterKey& k2) const
                {
                        return k1 < k2;
                }
        };

        class WSRouter
        {
        private:
                typedef std::function<void(conn_hdl_t, msg_ptr_t, const Json::Value&)> ws_func_t;

                std::map<WSRouterKey, ws_func_t, ws_less> func_map_;
                oldking::mymutex mtx_;

        public:
                WSRouter()
                : func_map_({})
                {}

                WSRouter(const WSRouter& other)
                : func_map_(other.func_map_)
                {}

                ~WSRouter()
                {}

                void addRouter(
                                const WSRouterKey& key,
                                ws_func_t func
                                )
                {
                        oldking::lock_guard lock(mtx_);
                        func_map_.insert({key, func});
                        return;
                }

                bool routing(conn_hdl_t hdl, msg_ptr_t msg)
                {
                        WS_ROUTER_LOG(LOG_INFO, "received websocket message");

                        if(msg == nullptr)
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message is null");
                                return false;
                        }

                        Json::Value req;
                        if(oldking::json_util::deserialize(msg->get_payload(), req) == false)
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message is not valid json");
                                return false;
                        }

                        if(!req["req_type"])
                        {
                                WS_ROUTER_LOG(LOG_ERROR, "websocket message has no req_type");
                                return false;
                        }

                        WSRouterKey key;
                        key.req_type = req["req_type"].asString();

                        ws_func_t func;
                        {
                                oldking::lock_guard lock(mtx_);
                                if(func_map_.find(key) == func_map_.end())
                                {
                                        WS_ROUTER_LOG(LOG_ERROR, "websocket route does not exist: " + key.req_type);
                                        return false;
                                }

                                func = func_map_[key];
                        }

                        func(hdl, msg, req);
                        return true;
                }
        };
}
