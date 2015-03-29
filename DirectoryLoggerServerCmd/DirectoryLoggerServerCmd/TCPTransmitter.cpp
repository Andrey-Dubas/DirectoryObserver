#include <fcntl.h>

#include "TCPTransmitter.h"
#include "Communicator.h"

size_t NetworkMessage::Length() const
{
	return (DWORD)*_content + sizeof(DWORD);
}

NetworkMessage NetworkMessage::operator=(const NetworkMessage& other)
{
	if(&other != this)
	{
		_content = new char[other.Length()];
		memcpy(_content, other.Serialize(), other.Length());
	}
	return *this;
}

NetworkMessage::NetworkMessage(const NetworkMessage& other)
{
	if(NULL != &other)
	{
		_content = new char[other.Length()];
		memcpy(_content, other.Serialize(), other.Length());
	}
}

NetworkMessage::NetworkMessage(const std::string& content)
{
	DWORD len = content.length();
	_content = new char[len + sizeof(DWORD)];
	memcpy(_content, &len, sizeof(len));
	char* payloadAddress = (char*)_content + sizeof(len);
	memcpy((char*)payloadAddress, content.data(), len); 
}

NetworkMessage::~NetworkMessage()
{
	delete [] _content;
}

const char* const NetworkMessage::Serialize() const
{
	return _content;
}

NetworkMessage NetworkMessage::Construct(const std::string& content)
{
	return NetworkMessage(content);
}


TCPTransmitter::TCPTransmitter(SOCKET socket): _socket(socket)
{
	bool enableNonBlocking;
    if (SOCKET_ERROR == ioctlsocket (_socket, FIONBIO, (unsigned long*)&enableNonBlocking)) 
    {
		throw NetworkException("can't initialise nonblocking");
    }
}

TCPTransmitter::~TCPTransmitter()
{
	
}

int TCPTransmitter::Send(const NetworkMessage& message)
{
	char mockData;
	if(0 == recv(_socket, &mockData, sizeof(mockData), 0)) // connection is closed
	{
		throw NetworkException("connection is closed by client");
	}
	int messageLen = message.Length();
	return send(_socket, message.Serialize(), messageLen, 0);
}
