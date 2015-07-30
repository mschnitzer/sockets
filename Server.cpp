#include "Server.h"
#include <iostream>

Server::Server(int server_id, std::string ip, int port, int protocol, AMX *amx)
{
	this->server_id = server_id;
	this->ip = ip;
	this->port = port;
	this->protocol = protocol;
	this->m_amx = amx;
}

#ifdef _WIN32
void Server::client_thread(int clientid, SOCKET sock)
#endif
{
	int res = 0;
	char buffer[2048];

	do {
		res = recv(sock, buffer, MAX_RECV_BUFFER, 0);
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

	// init
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		throw new SocketServerWSAStartupFailed();
	}

	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	
	std::string errstr;

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

	// server initialized
	this->server_initialized();

	SOCKADDR_IN client_info = { 0 };
	int addrsize = sizeof(client_info);
	
	// Accept a client socket
	while (client_socket = accept(srv_socket, (struct sockaddr*)&client_info, &addrsize))
	{
		if (client_socket == INVALID_SOCKET)
		{
			errstr = "Could not accept a new client!";
			this->send_server_error(SERVER_ERROR_ACCEPT_ERROR, errstr);
		}
		else
		{
			int clientid = this->get_free_client_id();
			if (clientid == -1)
			{
				errstr = "A new client tried to connect but client limit is reached!";
				this->send_server_error(SERVER_ERROR_CLIENT_LIMIT_REACHED, errstr);

				closesocket(client_socket);
				client_socket = INVALID_SOCKET;
			}
			else
			{
				client_ids.push_back(clientid);

				std::thread cThread(&Server::client_thread, this, clientid, client_socket);
				cThread.detach();
				
				this->clients.push_back({ clientid, cThread, client_socket });

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
#endif
}

void Server::close()
{
#ifdef _WIN32
	closesocket(this->srv_socket);
	this->srv_socket = INVALID_SOCKET;
#endif
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