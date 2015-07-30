#pragma once

class SocketServerWSAStartupFailed { };
class SocketServerGetaddrinfoFailed  { };
class SocketServerCreationFailed
{
public:
	int error;

	SocketServerCreationFailed(int error)
	{
		this->error = error;
	}
};
class SocketServerBindFailed
{
public:
	int error;

	SocketServerBindFailed(int error)
	{
		this->error = error;
	}
};
class SocketServerListenFailed
{
public:
	int error;

	SocketServerListenFailed(int error)
	{
		this->error = error;
	}
};