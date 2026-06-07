#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace oldking
{
	typedef uint64_t uid_t;
	typedef uint64_t rid_t;
	typedef std::string ssid_t;

	static std::unordered_map<std::string, std::string> mime_map =
	{
		{".html", "text/html; charset=UTF-8"},
		{".css", "text/css"},
		{".js", "application/javascript"},
		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".ico", "image/x-icon"}
	};

	static std::unordered_set<std::string> method_list =
	{
		"GET",
		"POST"
	};
}
