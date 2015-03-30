#pragma once

#include <list>
#include <vector>
#include <memory>

#include "TCPTransmitter.h"

std::string GetErrorString(int errorCode);

class NetworkException: public std::exception
{
public:
	NetworkException(const std::string& message): exception(message.data()){}
	const char * __CLR_OR_THIS_CALL what() const{return static_cast<exception>(*this).what();}
};

class Communicator
{
	Communicator(unsigned int port);
	static std::auto_ptr<Communicator> _instance;
	static DWORD WINAPI AcceptConnectionsThread(LPVOID params);
public:
	typedef TCPTransmitter Transmitter;
	typedef std::list<Transmitter> TransmitterList;
	typedef std::vector<NetworkMessage> MessageList;
public:
	virtual ~Communicator(void);
	static Communicator* Instance(unsigned int port);
	//static void DeleteInstance(){_instance.reset();}
	void Notify(const NetworkMessage& message);
	void AcceptConnection(); // async function
private:
	TransmitterList _channels;
	MessageList _messages;
	mutable HANDLE _mutex;
	SOCKET _acceptSocket;
	unsigned int _listenPort;
};
