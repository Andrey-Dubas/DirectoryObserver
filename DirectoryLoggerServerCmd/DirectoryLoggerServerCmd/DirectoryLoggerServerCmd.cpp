// DirectoryLoggerServerCmd.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <map>
#include <cstdlib>
#include <algorithm>
#include <signal.h>
#include <conio.h>

#include "Communicator.h"
#include "TCPTransmitter.h"

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

class Handle
{
	HANDLE _handle;
	Handle operator=(const Handle&){}
	Handle(const Handle&){}
public:
	explicit Handle(HANDLE handle): _handle(handle){}
	~Handle(){CloseHandle(_handle);}
	HANDLE Value(){return _handle;}
};

HANDLE ObservableDescriptor(LPCWSTR directoryPath)
{
	HANDLE hDir = CreateFile(directoryPath, 
			FILE_LIST_DIRECTORY,                
			FILE_SHARE_READ | FILE_SHARE_WRITE, 
			NULL,                               
			OPEN_EXISTING,                      
			FILE_FLAG_BACKUP_SEMANTICS,
			NULL                                
	);

	if(INVALID_HANDLE_VALUE == hDir)
	{
		throw std::exception(GetErrorString(GetLastError()).data());
	}
	return hDir;
}

DWORD WINAPI KeyboardInterrupt(void*)
{
	char pressedKey;
	do
	{
		while(!_kbhit());
		pressedKey = getch();
	} while('q' != pressedKey);

	exit(0);
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

std::pair<std::wstring, unsigned short> GetArguments(int argc, _TCHAR* argv[])
{
	if(argc > 2)
	{
		unsigned short port = getPort(argv[argc-1]);
		std::wstring directory;
		for(int i = 1; i < argc -1; ++i)
		{
			directory += std::wstring(argv[i]);
		}
		if(directory[0] == L'\'') directory = &directory[1];
		if(directory[0] == L'\"') directory = &directory[1];
		std::reverse(directory.begin(), directory.end());
		
		if(directory[0] == L'\'') directory = &directory[1];
		if(directory[0] == L'\"') directory = &directory[1];
		std::reverse(directory.begin(), directory.end());
		
		return std::pair<std::wstring, unsigned short>(directory, port);
	}
	throw ArgumentAmountException("expected two arguments");
}

int _tmain(int argc, _TCHAR* argv[])
{
	std::pair<std::wstring, unsigned short> args;
	char* help = "usage:\nAppName.exe <directory> <port>";
	try
	{
		args = GetArguments(argc, argv);
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

	int port = args.second;
	Communicator* comm = Communicator::Instance(port);
	comm->AcceptConnection(); // async operation

	LPCWSTR directoryPath = args.first.data();
	HANDLE hDir;
	try
	{
		hDir = ObservableDescriptor(directoryPath);
	}
	catch(std::exception& ex)
	{
		std::wstring wDirPath = directoryPath;
		std::cout << "error creating directory \'" 
			<< std::string(wDirPath.begin(), wDirPath.end()).data() 
			<< "\'descriptor: "<< ex.what() << std::endl;
		return -1;
	}

	std::cout << "Press q to exit" << std::endl;
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)KeyboardInterrupt, NULL, 0, NULL);

	Handle directoryDesc(hDir);

	FILE_NOTIFY_INFORMATION Buffer[1024];

	std::map<int, std::string> eventDescriptor;
	eventDescriptor[FILE_ACTION_ADDED] = "Added";
	eventDescriptor[FILE_ACTION_REMOVED] = "Removed";
	eventDescriptor[FILE_ACTION_MODIFIED] = "Modified";
	eventDescriptor[FILE_ACTION_RENAMED_OLD_NAME] = "Renamed from";
	eventDescriptor[FILE_ACTION_RENAMED_NEW_NAME] = "Renamed to";

	DWORD BytesReturned;
	while (true)
	{
		int ret = ReadDirectoryChangesW(
			directoryDesc.Value(),       // handle to directory
			&Buffer,      // read results buffer
			sizeof(Buffer),     // length of buffer
			TRUE,       // monitoring option
			
			FILE_NOTIFY_CHANGE_SECURITY |
			FILE_NOTIFY_CHANGE_SIZE |  // in file or subdir
			FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_DIR_NAME | // creating, deleting a directory or sub
			FILE_NOTIFY_CHANGE_FILE_NAME, // renaming,creating,deleting a file
			
			&BytesReturned,
			NULL,
			NULL 
		);

		int dirNameLength = Buffer[0].FileNameLength/2;
		std::wstring filenameW = 
			std::wstring(directoryPath) + L"\\" +
			std::wstring(Buffer[0].FileName, dirNameLength);
		std::string filename(filenameW.begin(), filenameW.end());
		comm->Notify(NetworkMessage(eventDescriptor[Buffer[0].Action] + ": " + filename));
	}

	return 0;
}

