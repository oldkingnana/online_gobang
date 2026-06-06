#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <jsoncpp/json/json.h>

#include "util.hpp"
#include "user_table.hpp"
#include "session_manager.hpp"

namespace oldking
{
	class HTTPServices
	{
	private:
		typedef websocketpp::http::parser::request http_req_t;
		typedef websocketpp::http::parser::response http_resp_t;

	public:
		static http_resp_t s_login(oldking::session_manager* smp, oldking::user_table* utp, const http_req_t& req)
		{
			http_resp_t res;
            Json::Value login_v; 
            oldking::json_util::deserialize(req.get_body(), login_v);

            Json::Value user_info;
            // 用户存在性检查
            if(utp->slct_by_name(login_v["usr_name"].asString(), user_info) == false)
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"用户不存在\"}");
                return res;
            }

            // 密码校验
            if(login_v["passwd"].asString() != user_info["passwd"].asString())
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"密码错误\"}");
                return res;
            }

            uint64_t uid = std::stoull(user_info["uid"].asString());
            std::string ssid;

            // 试图获取session 
            auto ss_ptr = smp->get_session(uid);
            if(ss_ptr == nullptr) 
            {
                ssid = smp->create_session(uid);
            }
            else 
            {
                if(ss_ptr->is_expired()) 
                {
                    // 如果已经过期,删除旧的并重新创建
                    smp->rm_session(ss_ptr->ssid());
                    ssid = smp->create_session(uid);
                }
                else 
                {
                    if (time(nullptr) - ss_ptr->last_access_time_ > (SESSION_TIMEOUT / 2)) {
                        ss_ptr->update_time();
                    }
                    ssid = ss_ptr->ssid();
                }
            }

            // 构建并返回http_resp_t 
            std::string cookie = "SSID=" + ssid + "; Path=/; HttpOnly";
            res.append_header("Set-Cookie", cookie);
            res.append_header("Content-Type", "application/json; charset=UTF-8");

            res.set_status(websocketpp::http::status_code::value::ok);
            res.set_body("{\"result\": true, \"reason\": \"登录成功\"}");
            
            return res;
		}

		// 注册完成之后默认为用户直接登录
		static http_resp_t s_register(oldking::session_manager* smp, oldking::user_table* utp, const http_req_t& req)
		{
			http_resp_t res;
            res.append_header("Content-Type", "application/json; charset=UTF-8");

            Json::Value register_v; 
            if(oldking::json_util::deserialize(req.get_body(), register_v) == false)
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"请求正文格式错误\"}");
                return res;
            }
			
			// 用户存在性校验
            if(!(register_v["usr_name"] && register_v["passwd"]))
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"用户名或密码为空\"}");
                return res;
            }

            Json::Value user_info;
            if(utp->slct_by_name(register_v["usr_name"].asString(), user_info))
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"用户已存在\"}");
                return res;
            }

            if(utp->rgst(register_v) == false)
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"注册失败\"}");
                return res;
            }

			// 新增session  
            if(utp->slct_by_name(register_v["usr_name"].asString(), user_info) == false)
            {
                res.set_status(websocketpp::http::status_code::value::forbidden);
                res.set_body("{\"result\": false, \"reason\": \"用户信息获取失败\"}");
                return res;
            }

            uint64_t uid = std::stoull(user_info["uid"].asString());
            std::string ssid = smp->create_session(uid);
			
			// 构建并返回http_resp_t 
            std::string cookie = "SSID=" + ssid + "; Path=/; HttpOnly";
            res.append_header("Set-Cookie", cookie);
            res.set_status(websocketpp::http::status_code::value::ok);
            res.set_body("{\"result\": true, \"reason\": \"注册成功\"}");

			return res;
		}

	};
}


