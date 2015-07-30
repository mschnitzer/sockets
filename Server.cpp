#include "Server.h"

Server::Server(int server_id, std::string ip, int port, int protocol, AMX *amx)
{
	this->server_id = server_id;
	this->ip = ip;
	this->port = port;
	this->protocol = protocol;
	this->m_amx = amx;
}

void Server::client_thread(int clientid, SOCKET sock)
{
	int res = 0;
	char buffer[2048];

	do {
#ifdef _WIN32
		res = recv(sock, buffer, MAX_RECV_BUFFER, 0);
#else
        res = read(sock, buffer, MAX_RECV_BUFFER);
#endif
		if (res > 0)
		{
			for (auto &i : this->clients)
			{
				if (i.clientid == clientid)
				{
					i.bytesSent += res;
					break;
				}
			}

			cell idx;

			if (!amx_FindPublic(this->m_amx, "OnSocketServerReceive", &idx))
			{
				cell addr;

				amx_Push(this->m_amx, res);
				amx_PushString(this->m_amx, &addr, NULL, buffer, NULL, NULL);
				amx_Push(this->m_amx, clientid);
				amx_Push(this->m_amx, this->server_id);

				amx_Exec(this->m_amx, NULL, idx);
			}
		}
	} while (res != 0);

	cell idx;

	if (!amx_FindPublic(this->m_amx, "OnSocketClientDisconnect", &idx))
	{
		cell addr;

		amx_Push(this->m_amx, 0);
		amx_Push(this->m_amx, clientid);
		amx_Push(this->m_amx, this->server_id);

		amx_Exec(this->m_amx, NULL, idx);
	}
}

void Server::start()
{
	this->max_clients = 500;

#ifdef _WIN32
	WSADATA wsaData;
	int iResult;

	SOCKET client_socket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int error = -1;
#else
	struct sockaddr_in serv_addr, client_info;
	this->srv_socket = socket(AF_INET, SOCK_STREAM, 0);
	int client_socket;
	std::string errstr;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Server Initialization
	//////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
	// init
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		throw new SocketServerWSAStartupFailed();
	}

	ZeroMemory(&hints, sizeof(hints));

	// fill data
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// resolve host
	char szPort[5] = { 0 };
	_itoa(port, szPort, 10);

	iResult = getaddrinfo(NULL, szPort, &hints, &result);
	if (iResult != 0)
	{
		WSACleanup();
		throw new SocketServerGetaddrinfoFailed();
	}

	// create socket
	this->srv_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (this->srv_socket == INVALID_SOCKET)
	{
		error = WSAGetLastError();
		freeaddrinfo(result);
		WSACleanup();

		throw new SocketServerCreationFailed(error);
	}

	// bind socket
	iResult = bind(this->srv_socket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		freeaddrinfo(result);
		closesocket(this->srv_socket);
		WSACleanup();

		throw new SocketServerBindFailed(error);
	}

	freeaddrinfo(result);

	// listen on socket
	iResult = listen(this->srv_socket, SOMAXCONN);
	if (iResult == SOCKET_ERROR)
	{
		error = WSAGetLastError();
		closesocket(srv_socket);
		WSACleanup();

		throw new SocketServerListenFailed(error);
	}

	SOCKADDR_IN client_info = { 0 };
	int addrsize = sizeof(client_info);
#else
	if (this->srv_socket < 0)
	{
		throw new SocketServerCreationFailed(this->srv_socket);
	}

	bzero((char *)&serv_addr, sizeof(serv_addr));

	// fill data
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	// bind socket
	int binderr = bind(this->srv_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (binderr < 0)
	{
		throw new SocketServerBindFailed(binderr);
	}

	// listen on socket
	listen(this->srv_socket, 5);

	unsigned int addrsize = sizeof(client_info);
#endif

	// server initialized callback
	this->server_initialized();

	// handle client connection requests
	while (client_socket = accept(this->srv_socket, (struct sockaddr *)&client_info, &addrsize))
	{
		if (client_socket == INVALID_SOCKET)
		{
			this->send_server_error(SERVER_ERROR_ACCEPT_ERROR, "Could not accept a new client!");
		}
		else
		{
			int clientid = this->get_free_client_id();
			if (clientid == -1)
			{
				this->send_server_error(SERVER_ERROR_CLIENT_LIMIT_REACHED, "A new client tried to connect but client limit is reached!");

#ifdef _WIN32
				closesocket(client_socket);
#else
				close(client_socket);
#endif
			}
			else
			{
				client_ids.push_back(clientid);

				std::thread cThread(&Server::client_thread, this, clientid, client_socket);
				cThread.detach();
				
				this->clients.push_back({ clientid, cThread, 0, 0, client_socket });

				cell idx;

				if (!amx_FindPublic(this->m_amx, "OnSocketClientConnect", &idx))
				{
					cell addr;
					
					char *ip = inet_ntoa(client_info.sin_addr);
					u_short rem_port = ntohs(client_info.sin_port);

					amx_Push(this->m_amx, rem_port);
					amx_PushString(this->m_amx, &addr, NULL, ip, NULL, NULL);
					amx_Push(this->m_amx, clientid);
					amx_Push(this->m_amx, this->server_id);

					amx_Exec(this->m_amx, NULL, idx);
				}
			}
		}
	}
}

void Server::shutdown()
{
#ifdef _WIN32
	closesocket(this->srv_socket);
#else
    close(this->srv_socket);
#endif
    this->srv_socket = INVALID_SOCKET;
}

int Server::get_free_client_id()
{
	int clientid = -1;

	for (int i = 0; i < this->max_clients; i++)
	{
		if (std::find(this->client_ids.begin(), this->client_ids.end(), i) != this->client_ids.end())
		{
			continue;
		}

		clientid = i;
		break;
	}

	return clientid;
}

void Server::send_server_error(int error, std::string errstr)
{
	cell idx;

	if (!amx_FindPublic(this->m_amx, "OnSocketServerError", &idx))
	{
		cell addr;

		amx_PushString(this->m_amx, &addr, NULL, errstr.c_str(), NULL, NULL);
		amx_Push(this->m_amx, error);
		amx_Push(this->m_amx, this->server_id);

		amx_Exec(this->m_amx, NULL, idx);
	}
}

void Server::server_initialized()
{
	cell idx;

	if (!amx_FindPublic(this->m_amx, "OnSocketServerInitialized", &idx))
	{
		amx_Push(this->m_amx, this->server_id);
		amx_Exec(this->m_amx, NULL, idx);
	}
}

void Server::set_max_clients(int max_clients)
{
	this->max_clients = max_clients;
}

bool Server::is_client_connected(int client_id)
{
	if (std::find(this->client_ids.begin(), this->client_ids.end(), client_id) != this->client_ids.end())
	{
		return true;
	}

	return false;
}

int Server::get_sent_client_bytes(int client_id)
{
	if (this->is_client_connected(client_id) == false)
	{
		return -1;
	}

	return this->clients[client_id].bytesSent;
}

int Server::get_received_client_bytes(int client_id)
{
	if (this->is_client_connected(client_id) == false)
	{
		return -1;
	}

	return this->clients[client_id].bytesRecevied;
}

void Server::socket_send_thread(int client_id, std::string text)
{
	if (this->is_client_connected(client_id) == true)
	{
		for (auto &i : this->clients)
		{
			if (i.clientid == client_id)
			{
				i.bytesRecevied += text.length();
			}
		}

#ifdef _WIN32
		send(clients[client_id].sock, text.c_str(), text.length(), 0);
#else
        write(clients[client_id].sock, text.c_str(), text.length());
#endif
	}
}

int Server::socket_send(int client_id, std::string text)
{
	if (this->is_client_connected(client_id) == false)
	{
		return 0;
	}

	std::thread(&Server::socket_send_thread, this, client_id, text).detach();
	return 1;
}
