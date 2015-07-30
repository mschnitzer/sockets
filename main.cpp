#define HAVE_STDINT_H
#define WIN32_LEAN_AND_MEAN

#include <string>
#include <thread>
#include "amx/amx.h"
#include "plugincommon.h"
#include "settings.h"
#include "Exceptions.h"
#include "Server.h"
#include "AmxUtils.h"

#ifdef _WIN32
#pragma comment (lib, "Ws2_32.lib")
#endif

s_ServerList servers[MAX_SERVERS];

typedef void(*logprintf_t)(char* format, ...);

logprintf_t logprintf;
extern void *pAMXFunctions;

AMX *m_amx;

bool is_server_used(int server_id)
{
	if (server_id < 0 || server_id >(MAX_SERVERS - 1))
	{
		return 0;
	}

	if (servers[server_id].isUsed == false)
	{
		return 0;
	}

	return 1;
}

void new_server(int server_id, std::string ip, int port, int protocol)
{
	int error = -1;
	std::string error_s;

	try
	{
		Server server(server_id, ip, port, protocol, m_amx);
		servers[server_id].sobject = &server;
		server.start();
	}
	catch (const SocketServerWSAStartupFailed &)
	{
		error_s = "WSAStartup failed!";
	}
	catch (const SocketServerGetaddrinfoFailed &)
	{
		error_s = "GetAddrInfo failed!";
	}
	catch (const SocketServerCreationFailed &e)
	{
		error_s = "Socket creation failed: " + e.error;
	}
	catch (const SocketServerBindFailed &e)
	{
		error_s = "Socket bind failed:" + e.error;
	}
	catch (const SocketServerListenFailed &e)
	{
		error_s = "Socket listen failed: " + e.error;
	}

	if (error_s.length() > 0)
	{
		logprintf("* sockets [ERROR]: %s", error_s);

		cell idx;

		if (!amx_FindPublic(m_amx, "OnSocketServerError", &idx))
		{
			cell addr;
			amx_PushString(m_amx, &addr, NULL, error_s.c_str(), NULL, NULL);
			amx_Push(m_amx, error);
			amx_Push(m_amx, server_id);

			amx_Exec(m_amx, NULL, idx);
		}
	}
}

cell AMX_NATIVE_CALL socket_server_start(AMX* amx, cell* params)
{
	//std::string ip = AmxUtils::amx_GetStdString(amx, &params[1]); - does currently not work for some reason?!
	cell *addr = NULL;
	int len = 0;

	amx_GetAddr(amx, params[1], &addr);
	amx_StrLen(addr, &len);
	len++;

	char *str = new char[len];
	amx_GetString(str, addr, 0, len);

	std::string ip = std::string(str);
	delete[] str;
	int port = params[2];
	int protocol = params[3];

	if (protocol != 1)
	{
		logprintf("* sockets [ERROR]: Invalid protocol specified: %d", protocol);
		return 0;
	}

	int server_id = -1;

	for (int i = 0; i < MAX_SERVERS; i++)
	{
		if (servers[i].isUsed == false)
		{
			server_id = i;
			break;
		}
	}
	
	if (server_id != -1)
	{
		servers[server_id].isUsed = true;
		std::thread(new_server, server_id, ip, port, protocol).detach();
	}
	else
	{
		logprintf("* sockets [ERROR]: Cannot create new server. Max server limit (%d) is reached.", MAX_SERVERS);
	}

	return server_id;
}

cell AMX_NATIVE_CALL socket_server_set_maxclients(AMX* amx, cell* params)
{
	int server_id = params[1];
	int max_clients = params[2];

	if (max_clients < 0) max_clients = 1;
	if (max_clients > 10000) max_clients = 10000;

	if (is_server_used(server_id) == false)
	{
		return 0;
	}

	servers[server_id].sobject->set_max_clients(max_clients);

	return 1;
}

cell AMX_NATIVE_CALL socket_get_client_info(AMX* amx, cell* params)
{
	int server_id = params[1];
	int client_id = params[2];

	if (is_server_used(server_id) == false)
	{
		return 0;
	}

	if (servers[server_id].sobject->is_client_connected(client_id) == false)
	{
		return 0;
	}

	cell *addr[2] = { NULL, NULL };

	amx_GetAddr(amx, params[3], &addr[0]); // bytes sent
	amx_GetAddr(amx, params[4], &addr[1]); // bytes received

	*addr[0] = servers[server_id].sobject->get_sent_client_bytes(client_id);
	*addr[1] = servers[server_id].sobject->get_received_client_bytes(client_id);

	return 1;
}

cell AMX_NATIVE_CALL socket_send(AMX* amx, cell* params)
{
	int server_id = params[1];
	int client_id = params[2];
	std::string text = AmxUtils::amx_GetStdString(amx, &params[3]);

	if (is_server_used(server_id) == false)
	{
		return 0;
	}

	if (servers[server_id].sobject->is_client_connected(client_id) == false)
	{
		return 0;
	}

	servers[server_id].sobject->socket_send(client_id, text);
	return 1;
}

cell AMX_NATIVE_CALL socket_is_client_connected(AMX* amx, cell* params)
{
	int server_id = params[1];
	int client_id = params[2];

	if (is_server_used(server_id) == false)
	{
		return 0;
	}

	return servers[server_id].sobject->is_client_connected(client_id);
}

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports()
{
	return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData)
{
	pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
	logprintf = (logprintf_t)ppData[PLUGIN_DATA_LOGPRINTF];

	logprintf(" --------------------------------------------------------");
	logprintf(" sockets plugin version %s successfully loaded!", PLUGIN_VERSION);
	logprintf(" Developer: Manuel Schnitzer");
	logprintf(" Website: https://github.com/mschnitzer/sockets");
	logprintf(" --------------------------------------------------------");

	return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload()
{
	logprintf(" * sockets plugin was unloaded.");
}

extern "C" const AMX_NATIVE_INFO PluginNatives[] =
{
	{ "socket_server_start", socket_server_start },
	{ "socket_server_set_maxclients", socket_server_set_maxclients },
	{ "socket_get_client_info", socket_get_client_info },
	{ "socket_send", socket_send },
	{ "socket_is_client_connected", socket_is_client_connected },
	{ 0, 0 }
};

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx)
{
	m_amx = amx;
	return amx_Register(amx, PluginNatives, -1);
}


PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx)
{
	return AMX_ERR_NONE;
}