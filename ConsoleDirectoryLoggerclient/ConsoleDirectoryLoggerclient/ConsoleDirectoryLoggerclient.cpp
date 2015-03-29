// ConsoleDirectoryLoggerclient.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <winsock.h>

#include <iostream>
#include <memory>
#include <cstdlib>
#include <vector>

typedef std::basic_string<_TCHAR> String;

const int SocketVersion = 0x0101;

class ArgumentException: public std::exception
{
public:
	ArgumentException(char* message): std::exception(message){}
};

class ArgumentAmountException: public std::exception
{
public:
	ArgumentAmountException(char* message): std::exception(message){}
};

class ClosedConnectionException: public std::exception
{
public:
	ClosedConnectionException(char* message): std::exception(message){}
};

class ErrorConnectionException: public std::exception
{
public:
	ErrorConnectionException(char* message): std::exception(message){}
};

class WSA
{
public:
	WSA()
	{
		WSADATA wsaData;
		if(WSAStartup(SocketVersion, &wsaData))
		{
			std::cout << "error initialising socket lib" << std::endl;
			throw std::exception("error ini WSA");
		}
	}
	~WSA()
	{
		WSACleanup();
	}
};

void at_exit()
{
	char c;
	std::cin >> c; 
}

void Receive(SOCKET receiver, char* data, size_t expectedBytes)
{
	while(expectedBytes > 0)
	{
		int result = recv(receiver, data, (int)expectedBytes, 0);

		if(-1 == result)
		{
			throw ErrorConnectionException("");
		}
		else if(0 == result)
		{
			throw ClosedConnectionException("");
		}
		else
		{
			expectedBytes -= result;
			data += result;
		}
	};
}

std::string ReceiveMessage(SOCKET receiver)
{
	DWORD messageLen = 0;
	Receive(receiver, (char*)&messageLen, sizeof(messageLen));
	
	std::vector<char> buffer(messageLen + 1);
	Receive(receiver, &buffer[0], (size_t)messageLen);
	buffer.push_back('\0');
	return std::string(&buffer[0]);

}

bool CheckIfNumber(const _TCHAR* portString)
{
	bool result = true;
	while(*portString != _T('\0'))
	{
		if(*portString < _T('0') || *portString > _T('9'))
		{
			result = false;
		}
		++portString;
	}
	return result;
}

void CheckIP(_TCHAR* ipStringRaw)
{
	std::basic_string<_TCHAR> ipString(ipStringRaw);
	std::basic_string<_TCHAR> number(_T(""));
	const wchar_t separator = _T('.');
	unsigned int downcounter = 4; // amount of numbers in IPV4
	
	for(String::iterator it = ipString.begin(); it != ipString.end(); ++it)
	{
		if(*it >= _T('0') && *it <= _T('9'))
		{
			number += *it;
		}
		else if(*it == separator)
		{
			if(CheckIfNumber(number.data()) == false)
			{
				throw std::exception("ip octet can't be recognised");
			}
			else
			{
				int octet = _ttoi(number.data());
				if(octet > 0xff && octet < 0)
				{
					throw ArgumentException("ip octet must be less then 256");
				}
			}
			number = _T("");
			downcounter--;
		}
	}

	CheckIfNumber(number.data());
	downcounter--;

	if(downcounter != 0)
	{
		throw ArgumentException("there are must be 4 octets");
	}
}
int getPort(_TCHAR* portString)
{
	if(false == CheckIfNumber(portString))
	{
		throw std::exception("port descriptor must be a number");
	}
	int port = _ttoi(portString);
	if(port > 0xffff)
	{
		throw std::exception("port must be from range 1..0xffff");
	}
	return port;
}

std::pair<std::string, unsigned short> GetArguments(int argc, _TCHAR* argv[])
{
	unsigned short port;
	String ip;
	if(argc == 2) // default IP = localhost
	{
		CheckIfNumber(argv[1]);
		port = _ttoi(argv[1]);
		ip = _T("127.0.0.1");
	}
	else if(argc > 2)
	{
		CheckIP(argv[1]);
		ip = argv[1];
		CheckIfNumber(argv[2]);
		port = _ttoi(argv[2]);
	}
	else
	{
		throw ArgumentAmountException("");
	}
	return std::pair<std::string, int>(std::string(ip.begin(), ip.end()), port);
}

int _tmain(int argc, _TCHAR* argv[])
{
	//std::atexit(at_exit);

	char* help = "usage: \nAppName.exe <IP> <port>\n  or\nAppName.exe <port> (for localhost)";
	std::pair<std::string, unsigned int> params;
	try
	{
		params = GetArguments(argc, argv);
	}
	catch(ArgumentException& ex)
	{
		std::cout << ex.what() << std::endl;
		std::cout << help;
		return -1;
	}
	catch(ArgumentAmountException& ex)
	{
		std::cout << help;
		return -1;
	}

	std::auto_ptr<WSA> wsa;
	try
	{
		wsa.reset(new WSA);
	}
	catch(std::exception& ex)
	{
		std::cout << "failed to init wsa" << std::endl;
		return -1;
	}
	
	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == s)
	{
		std::cout << "error creating socket" << std::endl;
		return -1;
	}
	sockaddr_in sockDetails;
	sockDetails.sin_port = params.second;
	sockDetails.sin_addr.S_un.S_addr = inet_addr(params.first.data());
	sockDetails.sin_family = AF_INET;

	if(0 != connect(s, (sockaddr*)&sockDetails, sizeof(sockDetails)))
	{
		std::cout << "error connecting" << std::endl;
		return -1;
	}

	while(1)
	{
		try
		{
			std::string message = ReceiveMessage(s);
			std::cout << "event: " << message.data() << std::endl;
		}
		catch(ErrorConnectionException& ex)
		{
			std::cout << "connection error: " << ex.what() << std::endl;
			break;
		}
		catch(ClosedConnectionException& ex)
		{
			std::cout << "Connection is closed by server" << std::endl;
			break;
		}
	}
	closesocket(s);

	return 0;
}

