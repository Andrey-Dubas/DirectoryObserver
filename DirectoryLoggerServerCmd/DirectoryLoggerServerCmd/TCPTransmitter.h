#pragma once
#include<string>
#include<windows.h>
/*
	haven't desided yet message structure, so I wrap it into message 
*/
class NetworkMessage
{
public:
	size_t Length() const;
	NetworkMessage operator=(const NetworkMessage&);
	NetworkMessage(const NetworkMessage&);
	NetworkMessage(const std::string&);
	const char* const Serialize() const;
	static NetworkMessage Construct(const std::string& content);
	~NetworkMessage();
private:
	char* _content;
};

class Communicator;

class TCPTransmitter
{
	friend class Communicator;
	SOCKET _socket;
private:
	explicit TCPTransmitter(SOCKET socket);
	void Deinit() {closesocket(_socket);}
public:
	virtual ~TCPTransmitter(void);
	int Send(const NetworkMessage& message);
};
