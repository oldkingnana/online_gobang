#pragma once

#include <unordered_map>
#include <string>
#include <unordered_set>

namespace oldking 
{
	// todo 可以后续把所有typedef都挪过来

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


