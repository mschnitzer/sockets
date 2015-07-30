#pragma once

#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#endif
#include "Exceptions.h"
#include "settings.h"
#include "amx/amx.h"
#include "ServerErrors.h"

#ifndef _WIN32
typedef int SOCKET;
#define INVALID_SOCKET 1
#endif

class Server {
private:
	int server_id;
	std::string ip;
	int port;
	int protocol;
	SOCKET srv_socket = INVALID_SOCKET;
	int max_clients;
	AMX *m_amx;

public:
	struct s_Clients
	{
		int clientid;
		std::thread &cthread;
		int bytesSent;
		int bytesRecevied;
		SOCKET sock;
	};

	std::vector<Server::s_Clients> clients;
	std::vector<int> client_ids;

	Server(int server_id, std::string ip, int port, int protocol, AMX *amx);
	void client_thread(int clientid, SOCKET sock);
	void start();
	void shutdown();
	int get_free_client_id();
	void send_server_error(int error, std::string errstr);
	void server_initialized();
	void set_max_clients(int max_clients);
	bool is_client_connected(int client_id);
	int get_sent_client_bytes(int client_id);
	int get_received_client_bytes(int client_id);
	void socket_send_thread(int client_id, std::string text);
	int socket_send(int client_id, std::string text);
};

struct s_ServerList
{
	Server *sobject;
	bool isUsed;
};
