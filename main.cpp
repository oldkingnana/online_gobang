#include "online_gobang_server.hpp"
#include "HTTPServices.hpp"

int main()
{
	oldking::session_manager session_manager;
	oldking::user_table user_table("127.0.0.1", "root", "", "Online_Gobang");
	oldking::online_gobang_server server;

	oldking::RouterKey login_key;
	login_key.method = "POST";
	login_key.path = "/login";
	server.http_add_router(login_key, [&session_manager, &user_table](const auto& req) {
		return oldking::HTTPServices::s_login(&session_manager, &user_table, req);
	});

	oldking::RouterKey register_key;
	register_key.method = "POST";
	register_key.path = "/register";
	server.http_add_router(register_key, [&session_manager, &user_table](const auto& req) {
		return oldking::HTTPServices::s_register(&session_manager, &user_table, req);
	});

	server.start();

	return 0;
}


