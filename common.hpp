#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace oldking
{
	// uid_t 是用户唯一标识类型,对应数据库 user.uid。
	typedef uint64_t uid_t;
	// rid_t 是房间唯一标识类型,由 room_manager 分配。
	typedef uint64_t rid_t;
	// ssid_t 是 session 凭证类型,会写入客户端 SSID Cookie。
	typedef std::string ssid_t;

	// 静态资源扩展名到 HTTP Content-Type 的映射表,供 HttpRouter 分发 wwwroot 文件时使用。
	static std::unordered_map<std::string, std::string> mime_map =
	{
		{".html", "text/html; charset=UTF-8"},
		{".css", "text/css"},
		{".js", "application/javascript"},
		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".ico", "image/x-icon"}
	};

	// 当前项目允许识别的 HTTP 方法集合,后续如果做方法合法性检查可以直接复用。
	static std::unordered_set<std::string> method_list =
	{
		"GET",
		"POST"
	};
}
