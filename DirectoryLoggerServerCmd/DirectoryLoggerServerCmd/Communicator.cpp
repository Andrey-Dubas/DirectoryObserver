#include "Communicator.h"
#include <winsock.h>
#include <assert.h>

#include <iostream>

const int socketVersion = 0x0101;
std::auto_ptr<Communicator> Communicator::_instance(NULL);

std::string GetErrorString(int errorCode)
{
	const int MessageLength = 256;
	char errorMessage[MessageLength];
	::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                     NULL,
                     errorCode,
                     MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
                     errorMessage,
                     MessageLength - 1,
                     NULL);
	return std::string(errorMessage);
}

class AcceptThreadParam
{
	typedef Communicator::TransmitterList* TransmitterListRef;
	typedef Communicator::MessageList* MessageListRef;

	HANDLE _mutex;
	TransmitterListRef _connections;
	MessageListRef _messages;
	SOCKET _acceptSocket;
public:
	AcceptThreadParam(HANDLE mutex, TransmitterListRef connections, MessageListRef messages, SOCKET acceptSocket):
	  _mutex(mutex), _connections(connections), _messages(messages), _acceptSocket(acceptSocket)
	{
		assert(_mutex);
		assert(_connections);
		assert(_acceptSocket);
		assert(_messages);
	}
	const TransmitterListRef ChannelList(){return _connections;}
	const MessageListRef MessageList(){return _messages;}
	HANDLE Mutex(){return _mutex;}
	SOCKET Socket(){return _acceptSocket;}
};

DWORD WINAPI Communicator::AcceptConnectionsThread(LPVOID rawParams)
{
	sockaddr_in communicate_sockaddr;
	int len = sizeof(communicate_sockaddr);
	AcceptThreadParam* params = static_cast<AcceptThreadParam*>(rawParams);

	while(true)
	{
		SOCKET communicateSocket = accept(params->Socket(), (sockaddr*)&communicate_sockaddr, &len);
		if(INVALID_SOCKET == communicateSocket)
		{
			std::string strError = GetErrorString(WSAGetLastError());
			int i = 0;
		}
		TCPTransmitter channel(communicateSocket);

		DWORD waitResult = WaitForSingleObject(params->Mutex(), INFINITE);
		switch(waitResult)
		{
		case WAIT_OBJECT_0:
			for(MessageList::iterator it = params->MessageList()->begin(); it != params->MessageList()->end(); ++it) // send algeady created events
			{
				channel.Send(*it);
			}
			params->ChannelList()->push_back(channel);
			ReleaseMutex(params->Mutex());
			break;
		default:
			break;
		}
	}
}

Communicator::Communicator(unsigned int port): _listenPort(port)
{
	this->_mutex = CreateMutex(NULL, false, NULL);
}

Communicator::~Communicator(void)
{
	CloseHandle(_mutex);
	for(TransmitterList::iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		it->Deinit();
	}
}

Communicator* Communicator::Instance(unsigned int port)
{
	if(NULL == _instance.get())
	{
		_instance.reset(new Communicator(port));
	}
	return _instance.get();
}

void Communicator::Notify(const NetworkMessage& message)
{
	DWORD result = WaitForSingleObject(_mutex, INFINITE);
	switch(result)
	{
	case WAIT_OBJECT_0:
		typedef std::vector<TransmitterList::iterator> TransmitterReferenceList;
		TransmitterReferenceList closedConnection;
		for(TransmitterList::iterator it = _channels.begin(); it != _channels.end(); ++it)
		{
			try
			{
				it->Send(message);
			}
			catch(NetworkException&)
			{
				closedConnection.push_back(it);
			}
		}
		for(TransmitterReferenceList::iterator it = closedConnection.begin(); it != closedConnection.end(); ++it)
		{
			_channels.erase(*it);
		}
		_messages.push_back(message);
		ReleaseMutex(_mutex);
		break;
	}
}

void Communicator::AcceptConnection()
{
	WSADATA wsaData;
	if(WSAStartup(socketVersion, &wsaData))
	{
		throw NetworkException(GetErrorString(WSAGetLastError()));
	}
	_acceptSocket = socket(AF_INET, SOCK_STREAM, 0);

	if(0 == _acceptSocket)
	{
		throw NetworkException(std::string(
			"error while initialising accept socket: ")+
			GetErrorString(WSAGetLastError()));
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = _listenPort;
	serverAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if(0 != bind(_acceptSocket, (sockaddr*)(&serverAddr), sizeof(serverAddr)))
	{
		throw NetworkException("Error listen socket binding: " + GetErrorString(WSAGetLastError()));
	}

	if(0 != listen(_acceptSocket, 1))
	{
		throw NetworkException("Error listen socket binding: " + GetErrorString(WSAGetLastError()));
	}

	AcceptThreadParam* params = new AcceptThreadParam(_mutex, &_channels, &_messages, _acceptSocket);

	DWORD threadId;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AcceptConnectionsThread, params, 0, &threadId);
}