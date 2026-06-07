#pragma once

#include <jsoncpp/json/json.h>

#include "util.hpp"
#include "net_common.hpp"
#include "session_manager.hpp"
#include "session_detector.hpp"
#include "online_manager.hpp"
#include "matcher.hpp"
#include "room_manager.hpp"
#include "user_table.hpp"

namespace oldking
{
	class WebSocketServices
	{
	public:
		WebSocketServices() = delete;
		~WebSocketServices() = delete;
	};
}
