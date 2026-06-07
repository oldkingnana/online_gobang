#pragma once

#include <jsoncpp/json/json.h>

#include "util.hpp"
#include "net_common.hpp"
#include "user_table.hpp"
#include "session_manager.hpp"

namespace oldking
{
	// HTTPServices 是 HTTP 业务函数集合,它本身不保存业务状态,只负责把一个 HTTP 请求转换成一个 HTTP 响应。
	// 这个类中的函数会被注册到 HttpRouter 中,由 HTTP 路由根据 method/path 调用。
	// 当前它主要承担登录和注册两类入口业务:解析前端 JSON、检查用户表、创建或复用 session、写入 SSID Cookie,
	// 然后用统一的 JSON 文本向前端返回 result/reason。
	// 它不负责静态文件分发,也不负责后续大厅/房间的登录态判断;后续请求的身份过滤由 session_detector 和 WebSocketServices 处理。
	class HTTPServices
	{
	private:
		// 构造 HTTP JSON 响应体,统一返回 result/reason,并设置 application/json 响应头。
		static void set_json_body(http_resp_t& res, bool result, const std::string& reason)
		{
			Json::Value body;
			body["result"] = result;
			body["reason"] = reason;

			std::string body_str;
			oldking::json_util::serialize(body, body_str);
			res.append_header("Content-Type", "application/json; charset=UTF-8");
			res.set_body(body_str);
		}

	public:
		// 登录业务函数:解析 usr_name/passwd,校验数据库用户信息,为合法用户创建或复用 session,并通过 Set-Cookie 下发 SSID。
		static http_resp_t s_login(oldking::session_manager* smp, oldking::user_table* utp, const http_req_t& req)
		{
			http_resp_t res;
			Json::Value login_v;
			if(oldking::json_util::deserialize(req.get_body(), login_v) == false)
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "invalid request body");
				return res;
			}

			if(login_v["usr_name"].isNull() || login_v["passwd"].isNull())
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "usr_name or passwd is empty");
				return res;
			}

			Json::Value user_info;
			if(utp->slct_by_name(login_v["usr_name"].asString(), user_info) == false)
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "user does not exist");
				return res;
			}

			if(login_v["passwd"].asString() != user_info["passwd"].asString())
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "passwd is wrong");
				return res;
			}

			uid_t uid = std::stoull(user_info["uid"].asString());
			ssid_t ssid;

			auto ss_ptr = smp->get_session(uid);
			if(ss_ptr == nullptr)
			{
				ssid = smp->create_session(uid);
			}
			else
			{
				if(ss_ptr->is_expired())
				{
					smp->rm_session(ss_ptr->ssid());
					ssid = smp->create_session(uid);
				}
				else
				{
					if(time(nullptr) - ss_ptr->last_access_time_ > (SESSION_TIMEOUT / 2))
					{
						ss_ptr->update_time();
					}
					ssid = ss_ptr->ssid();
				}
			}

			std::string cookie = "SSID=" + ssid + "; Path=/; HttpOnly";
			res.append_header("Set-Cookie", cookie);
			res.set_status(websocketpp::http::status_code::value::ok);
			set_json_body(res, true, "login success");
			return res;
		}

		// 注册业务函数:解析 usr_name/passwd,检查用户名是否重复,写入数据库,注册成功后直接创建登录态 session 并下发 SSID。
		static http_resp_t s_register(oldking::session_manager* smp, oldking::user_table* utp, const http_req_t& req)
		{
			http_resp_t res;

			Json::Value register_v;
			if(oldking::json_util::deserialize(req.get_body(), register_v) == false)
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "invalid request body");
				return res;
			}

			if(register_v["usr_name"].isNull() || register_v["passwd"].isNull())
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "usr_name or passwd is empty");
				return res;
			}

			Json::Value user_info;
			if(utp->slct_by_name(register_v["usr_name"].asString(), user_info))
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "user already exists");
				return res;
			}

			if(utp->rgst(register_v) == false)
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "register failed");
				return res;
			}

			if(utp->slct_by_name(register_v["usr_name"].asString(), user_info) == false)
			{
				res.set_status(websocketpp::http::status_code::value::forbidden);
				set_json_body(res, false, "fetch user info failed");
				return res;
			}

			uid_t uid = std::stoull(user_info["uid"].asString());
			ssid_t ssid = smp->create_session(uid);

			std::string cookie = "SSID=" + ssid + "; Path=/; HttpOnly";
			res.append_header("Set-Cookie", cookie);
			res.set_status(websocketpp::http::status_code::value::ok);
			set_json_body(res, true, "register success");
			return res;
		}
	};
}
