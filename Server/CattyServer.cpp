
#pragma warning (disable:4127)

#ifdef _IA64_
#pragma warning(disable:4267)
#endif 

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#define xmalloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define xfree(p)   HeapFree(GetProcessHeap(),0,(p))

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../Inc/CattyProtocol.h"
#include "../Inc/ThreadPool.h"
#include "CattyServer.h"

using std::vector;

char* g_Port = DEFAULT_PORT;
bool g_bEndServer = false;			// set to TRUE on CTRL-C
bool g_bRestart = true;				// set to TRUE to CTRL-BRK
bool g_bVerbose = false;
// unsigned int g_dwThreadCount = 0;		//worker thread count
int fd_listen = INVALID_SOCKET;

ThreadPool pool(MAX_WORKER_THREAD);
extern std::unordered_map<int, PPER_SOCKET_CONTEXT> AllConnections;

// PPER_SOCKET_CONTEXT g_pCtxtList = NULL;		// linked list of context info structures

int myprintf(const char* lpFormat, ...);

int main(int argc, char* argv[]) {
	int fd_conn = INVALID_SOCKET;
	int fd_max = INVALID_SOCKET;
	vector<PPER_SOCKET_CONTEXT> sockets;

	// if (!ValidOptions(argc, argv))
	// 	return 1;

	while (g_bRestart) {
		g_bRestart = false;
		g_bEndServer = false;

		if (!CreateListenSocket()) {
			return 1;
		}

		fd_max = fd_listen;
		fd_set read_set, exc_set;
		int events = 0;
		FD_ZERO(&read_set);
		FD_ZERO(&exc_set);
		FD_SET(fd_listen, &read_set);
			
		while (true) {
			PPER_SOCKET_CONTEXT new_ctxt = NULL;
			events = select(fd_max + 1, &read_set, NULL, &exc_set, NULL);

			if (FD_ISSET(fd_listen, &read_set)) {
				struct sockaddr_in client_addr; 
				socklen_t client_len = sizeof(client_addr); 

				fd_conn = accept(fd_listen, (sockaddr*) &client_addr, &client_len);
				if (fd_conn == SOCKET_ERROR) {
					return 1;
				}

				FD_SET(fd_conn, &read_set);
				if (fd_conn > fd_max) {
					fd_max = fd_conn;
				}

				new_ctxt = UpdateCompletionPort(fd_conn, true);
				if (new_ctxt == NULL) {
					return 1; //TODO: Change to loop break handle
				}

				if (g_bEndServer)
					break;

				if (--events <= 0) {
					continue;
				}
			}

			for (auto& context : sockets) {
				int fd = context->Socket;
				if (FD_ISSET(fd, &read_set) && !context->IOActive) {
					context->IOActive = true;
					if (context->pIOContext == NULL) {
						PPER_IO_CONTEXT pIOContext = AllocIOContext(context);
						if (pIOContext == NULL) {
							return 1; //TODO: Handle this
						}
						context->pIOContext = pIOContext;
					}

					pool.enqueue(WorkerThread, context); 
				}

				if (--events <= 0) {
					break;
				}
			}

			// A new client connected earlier: Add them to active connections 
			if (new_ctxt != NULL) {
				sockets.push_back(new_ctxt);
				AllConnections[fd_conn] = new_ctxt;
			}
		} 

		g_bEndServer = true;
		//CtxtListFree();

		if (fd_listen != INVALID_SOCKET) {
			close(fd_listen);
			fd_listen = INVALID_SOCKET;
		}

		if (fd_conn != INVALID_SOCKET) {
			close(fd_conn);
			fd_conn = INVALID_SOCKET;
		}

		if (g_bRestart) {
			printf("\niocpserver is restarting...\n");
		}
		else {
			printf("\niocpserver is exiting...\n");
		}
	} //while (g_bRestart)

	// TODO: final Cleanup
	return 0;
} //main      

//
//  Just validate the command line options.
//
bool ValidOptions(int argc, char* argv[]) {

	// bool bRet = true;

	// for (int i = 1; i < argc; i++) {
	// 	if ((argv[i][0] == '-') || (argv[i][0] == '/')) {
	// 		switch (tolower(argv[i][1])) {
	// 		case 'e':
	// 			if (strlen(argv[i]) > 3)
	// 				g_Port = &argv[i][3];
	// 			break;

	// 		case 'v':
	// 			g_bVerbose = true;
	// 			break;

	// 		case '?':
	// 			fprintf("Usage:\n  iocpserver [-p:port] [-v] [-?]\n");
	// 			fprintf("  -e:port\tSpecify echoing port number\n");
	// 			fprintf("  -v\t\tVerbose\n");
	// 			fprintf("  -?\t\tDisplay this help\n");
	// 			bRet = false;
	// 			break;

	// 		default:
	// 			fprintf("Unknown options flag %s\n", argv[i]);
	// 			bRet = false;
	// 			break;
	// 		}
	// 	}
	// }

	// return(bRet);
}

//
//  Intercept CTRL-C or CTRL-BRK events and cause the server to initiate shutdown.
//  CTRL-BRK resets the restart flag, and after cleanup the server restarts.
//
bool CtrlHandler(unsigned int dwEvent) {

	// int sockTemp = INVALID_SOCKET;

	// switch (dwEvent) {
	// case CTRL_BREAK_EVENT:
	// 	g_bRestart = true;
	// case CTRL_C_EVENT:
	// case CTRL_LOGOFF_EVENT:
	// case CTRL_SHUTDOWN_EVENT:
	// case CTRL_CLOSE_EVENT:
	// 	if (g_bVerbose)
	// 		myprintf("CtrlHandler: closing listening socket\n");

	// 	//
	// 	// cause the accept in the main thread loop to fail
	// 	//

	// 	//
	// 	//We want to make closesocket the last call in the handler because it will
	// 	//cause the WSAAccept to return in the main thread
	// 	//
	// 	sockTemp = fd_listen;
	// 	fd_listen = INVALID_SOCKET;
	// 	g_bEndServer = true;
	// 	closesocket(sockTemp);
	// 	sockTemp = INVALID_SOCKET;
	// 	break;

	// default:
	// 	// unknown type--better pass it on.
	// 	return false;
	// }
	// return(true);
}

//
//  Create a listening socket.
//
bool CreateListenSocket(void) {
	int res = 0;
	int nZero = 0;

	//memset(&hints, 0, sizeof(struct addrinfo));
	struct addrinfo hints = { 0 };

	struct addrinfo* addrlocal = NULL;

	//
	// Resolve the interface
	//
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_IP;

	if (getaddrinfo(NULL, g_Port, &hints, &addrlocal) != 0) {
		fprintf(stderr, "getaddrinfo() failed with error %d\n");
		return(false);
	}

	if (addrlocal == NULL) {
		fprintf(stderr, "getaddrinfo() failed to resolve/convert the interface\n");
		return false;
	}

	fd_listen = socket(addrlocal->ai_family, addrlocal->ai_socktype, addrlocal->ai_protocol);
	if (fd_listen == INVALID_SOCKET) {
		fprintf(stderr, "WSASocket(fd_listen) failed: %d\n");
		return(false);
	}

	res = bind(fd_listen, addrlocal->ai_addr, addrlocal->ai_addrlen);
	if (res == SOCKET_ERROR) {
		fprintf(stderr, "bind() failed: %d\n");
		return(false);
	}

	res = listen(fd_listen, 5);
	if (res == SOCKET_ERROR) {
		fprintf(stderr, "listen() failed: %d\n");
		return(false);
	}

	//
	// Disable send buffering on the socket.  Setting SO_SNDBUF
	// to 0 causes winsock to stop buffering sends and perform
	// sends directly from our buffers, thereby reducing CPU usage.
	//
	// However, this does prevent the socket from ever filling the
	// send pipeline. This can lead to packets being sent that are
	// not full (i.e. the overhead of the IP and TCP headers is 
	// great compared to the amount of data being carried).
	//
	// Disabling the send buffer has less serious repercussions 
	// than disabling the receive buffer.
	//
	nZero = 0;
	res = setsockopt(fd_listen, SOL_SOCKET, SO_SNDBUF, (char*)&nZero, sizeof(nZero));
	if (res == SOCKET_ERROR) {
		fprintf(stderr, "setsockopt(SNDBUF) failed: %d\n");
		return(false);
	}

	freeaddrinfo(addrlocal);
	return(true);
}

//
// Worker thread that handles all I/O requests on any socket handle added to the IOCP.
//
unsigned int WorkerThread(void* WorkThreadContext) {
	bool bSuccess = false;
	int res = 0;
	PPER_SOCKET_CONTEXT socket_context = (PPER_SOCKET_CONTEXT) WorkThreadContext;
	PPER_IO_CONTEXT io_context = socket_context->pIOContext;

	if (io_context->IoContextType.Type.TcpMarker) {
		res = read(socket_context->Socket, (void*) io_context->InBufSize, 4);
		socket_context->IOActive = false;
		if (res != 4) {
			return 0;
		}

		unsigned int PayloadSize = ntohl(io_context->InBufSize);
		if (PayloadSize > 8 * MB) {
			CloseClient(socket_context, false);
		}
		else {
			int Sock = socket_context->Socket;
			io_context->InBuffer = (char*)malloc(PayloadSize);

			if (io_context->InBuffer == NULL) {
				CloseClient(socket_context, false);
			}
			else {
				io_context->IOOperation = ClientIoRead;
				io_context->nTotalBytes = PayloadSize; // ?
				io_context->nSentBytes = 0;
				io_context->IoContextType.Type.TcpMarker = false;
			}

		}
	} else { //handle command execution
		res = read(socket_context->Socket, 
				(void*) (io_context->InBuffer + (io_context->InBufSize - io_context->nTotalBytes)),
				io_context->nTotalBytes);
		socket_context->IOActive = false;

		if (res != io_context->nTotalBytes) {
			if (res == SOCKET_ERROR) {
				if (errno != EAGAIN && errno != EINTR) {
					return 1;
				}
			} else {
				io_context->nTotalBytes -= res;
			}
			return 0;
		}
		
		socket_context->pIOContext = NULL;
		int Result = DecodeIOContext(io_context);
		if (Result == 0) {
			Result = ProcessIOContext(io_context);
		}

		if (Result != 0) {
			CloseClient(socket_context, false);
		}

	}

	return 0;
}

int DecodeIOContext(PPER_IO_CONTEXT pIOContext) {
	char* CurrentPtr = pIOContext->InBuffer;
	unsigned int Size = pIOContext->InBufSize;
	int Result = SUCCESS;
	if (Size >= 4) {
		unsigned int Magic = ntohl(*(unsigned int*)CurrentPtr);
		if (Magic != MAGIC) {
			Result = DECODE_FAILURE;
		}
	}
	else {
		Result = DECODE_FAILURE;
	}

	unsigned long TransID = 0;
	if (Result == SUCCESS) {
		CurrentPtr += 4; 
		Size -= 4; 
		if (Size >= 4) {
			TransID = ntohl(*(unsigned long*)CurrentPtr);
		}
		else {
			Result = DECODE_FAILURE;
		}
	}

	bool IsReq = false; 
	if (Result == SUCCESS) {
		CurrentPtr += 4;
		Size -= 4;
		if (Size >= 4) {
			IsReq = ntohl(*(unsigned int*)CurrentPtr) == 0 ? true : false;
		}
		else {
			Result = DECODE_FAILURE;
		}
	}

	if (!IsReq) {
		Result = DECODE_FAILURE;
	}

	if (Result == SUCCESS) {
		CurrentPtr += 4;
		Size -= 4;

		if (Size >= 4) {
			unsigned int Command = ntohl(*(unsigned int*)CurrentPtr);
			unsigned int Capacity = 0;
			unsigned int NameSize = 0; 
			RoomID Destination = 0; 
			UserID Receiver = 0;

			switch (Command) { // if res and not send message then failure?
			case CreateRoom:
				CurrentPtr += 4;
				Size -= 4;
				if (Size >= 4) {
					Capacity = ntohl(*(unsigned int*)CurrentPtr);
					
				}


				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4; 
					if (Size >= 4) {
						NameSize = ntohl(*(unsigned int*)CurrentPtr);
					}

					if (NameSize == 0 || NameSize > 128) {
						Result = DECODE_FAILURE;
					}
				}

				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4;
					if (Size >= NameSize) {
						CreateRoomReq* NewRoom = new CreateRoomReq(CurrentPtr, Capacity, TransID);
						if (NewRoom) {
							pIOContext->Request = NewRoom;
							pIOContext->IoContextType.Type.Decoded = true;
						}
						else {
							Result = DECODE_FAILURE;
						}
						
					}
					else {
						Result = DECODE_FAILURE;
					}
					
				} 
				break;
		
			case JoinRoom: 
				CurrentPtr += 4; 
				Size -= 4; 
				
				if (Size >= 4) {
					Destination = ntohl(*(RoomID*)CurrentPtr);
				}

				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4; 
					if (Size >= 4) {
						NameSize = ntohl(*(unsigned int*)CurrentPtr); 
					}

					if (NameSize == 0 || NameSize > 128) {
						Result = DECODE_FAILURE;
					}
				}

				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4; 
					if (Size >= NameSize) {
						JoinRoomReq* RmJoin = new JoinRoomReq(CurrentPtr, Destination, TransID);
						if (RmJoin) {
							pIOContext->Request = RmJoin; 
							pIOContext->IoContextType.Type.Decoded = true;
						}
						else {
							Result = DECODE_FAILURE;
						}
					}
				}
				break;

			case SendChat:
				CurrentPtr += 4; 
				Size -= 4; 
				if (Size >= 4) {
					Receiver = ntohl(*(UserID*)CurrentPtr);
				}


				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4; 
					if (Size >= 4) {
						NameSize = ntohl(*(unsigned int*)CurrentPtr);
					}

					if (NameSize == 0 || NameSize >= 1000) { //temporary upper limiting 1k chars
						Result = DECODE_FAILURE;
					}
				}

				if (Result == SUCCESS) {
					CurrentPtr += 4; 
					Size -= 4; 
					if (Size >= NameSize) {
						SendMessageReq* Send = new SendMessageReq(CurrentPtr, Receiver, TransID);
						if (Send) {
							pIOContext->Request = Send; 
							pIOContext->IoContextType.Type.Decoded = true; 
						}
						else {
							Result = DECODE_FAILURE;
						}
					}
				}
				break;

			default: 
				Result = DECODE_FAILURE;

			}

		}
		else {
			Result = DECODE_FAILURE;
		}

	
		
	}
	return Result;
}

int ProcessIOContext(PPER_IO_CONTEXT pIOContext) {
	int Result = SUCCESS; 
	if (pIOContext->Request->IsValidReq()) {
		pIOContext->Response = pIOContext->Request->Execute();
		if (pIOContext->Response == nullptr) {
			Result = INSUFFICIENT_RESOURCES;
		}
	}

	if (Result == SUCCESS) {
		pIOContext->OutBufSize = pIOContext->Response->GetBufSize();
		pIOContext->OutBuffer = (char*)malloc(pIOContext->OutBufSize); //xfree in destruction!! 
		if (pIOContext->OutBuffer == nullptr) {
			Result = INSUFFICIENT_RESOURCES;
		}
	}

	if (Result == SUCCESS) {
		Result = pIOContext->Response->Encode(pIOContext->OutBuffer, pIOContext->OutBufSize); 
		
	}

	if (Result == SUCCESS) {
		pIOContext->IoContextType.Type.Encoded = true;
		pIOContext->IOOperation = ClientIoWrite;

		pIOContext->pConnection->SendLock.lock();
		if (SendUtil(pIOContext->pConnection->Socket, 
						pIOContext->OutBufSize, pIOContext->OutBuffer) != SUCCESS) {
			pIOContext->pConnection->SendLock.unlock();
			return 1;
		}
		pIOContext->pConnection->SendLock.unlock();
	}
	else {
		SendGeneralFailureResponse(pIOContext, Result); //scratch buffer
	}
	
	return Result;
}

int SendGeneralFailureResponse(PPER_IO_CONTEXT pIOContext, int Result) {
	return 0; //chang elater
}


//
//  Allocate a context structures for the socket and add the socket to the IOCP.  
//  Additionally, add the context structure to the global list of context structures.
//  Negative AdditionalSize is TCP Marker processing
//
PPER_SOCKET_CONTEXT UpdateCompletionPort(int sd, bool bAddToList) {

	PPER_SOCKET_CONTEXT lpPerSocketContext;


	lpPerSocketContext = CtxtAllocate(sd);
	if (lpPerSocketContext == NULL)
		return(NULL);

	// g_hIOCP = CreateIoCompletionPort((HANDLE)sd, g_hIOCP, (DWORD_PTR)lpPerSocketContext, 0);
	// if (g_hIOCP == NULL) {
	// 	myprintf("CreateIoCompletionPort() failed: %d\n", GetLastError());
	// 	if (lpPerSocketContext->pIOContext)
	// 		free(lpPerSocketContext->pIOContext);
	// 	free(lpPerSocketContext);
	// 	return(NULL);
	// }

	//
	//The listening socket context (bAddToList is FALSE) is not added to the list.
	//All other socket contexts are added to the list.
	//
	// if (bAddToList) CtxtListAddTo(lpPerSocketContext);

	// if (g_bVerbose)
	// 	fprintf("UpdateCompletionPort: Socket(%d) added to IOCP\n", lpPerSocketContext->Socket);

	return(lpPerSocketContext);
}

//
//  Close down a connection with a client.  This involves closing the socket (when 
//  initiated as a result of a CTRL-C the socket closure is not graceful).  Additionally, 
//  any context data associated with that socket is free'd.
//
void CloseClient(PPER_SOCKET_CONTEXT lpPerSocketContext,
	bool bGraceful) {

	// __try
	// {
	// 	EnterCriticalSection(&g_CriticalSection);
	// }
	// __except (EXCEPTION_EXECUTE_HANDLER)
	// {
	// 	myprintf("EnterCriticalSection raised an exception.\n");
	// 	return;
	// }

	// if (lpPerSocketContext) {
	// 	if (g_bVerbose)
	// 		myprintf("CloseClient: Socket(%d) connection closing (graceful=%s)\n",
	// 			lpPerSocketContext->Socket, (bGraceful ? "TRUE" : "FALSE"));
	// 	if (!bGraceful) {

	// 		//
	// 		// force the subsequent closesocket to be abortative.
	// 		//
	// 		LINGER  lingerStruct;

	// 		lingerStruct.l_onoff = 1;
	// 		lingerStruct.l_linger = 0;
	// 		setsockopt(lpPerSocketContext->Socket, SOL_SOCKET, SO_LINGER,
	// 			(char*)&lingerStruct, sizeof(lingerStruct));
	// 	}
	// 	closesocket(lpPerSocketContext->Socket);
	// 	lpPerSocketContext->Socket = INVALID_SOCKET;
	// 	CtxtListDeleteFrom(lpPerSocketContext);
	// 	lpPerSocketContext = NULL;
	// }
	// else {
	// 	myprintf("CloseClient: lpPerSocketContext is NULL\n");
	// }

	// LeaveCriticalSection(&g_CriticalSection);

	return;
}

//
// Allocate a socket context for the new connection.  
//
PPER_SOCKET_CONTEXT CtxtAllocate(int sd) {

	PPER_SOCKET_CONTEXT lpPerSocketContext;


	lpPerSocketContext = (PPER_SOCKET_CONTEXT)malloc(sizeof(PER_SOCKET_CONTEXT));
	if (lpPerSocketContext) {
		lpPerSocketContext->Socket = sd;
		// lpPerSocketContext->pCtxtBack = NULL;
		// lpPerSocketContext->pCtxtForward = NULL;
	}
	else {
		fprintf(stderr, "HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n");
	}

	return(lpPerSocketContext);
}

PPER_IO_CONTEXT AllocIOContext(PPER_SOCKET_CONTEXT Connection) {
	PPER_IO_CONTEXT pIOContext = (PPER_IO_CONTEXT)malloc(sizeof(PER_IO_CONTEXT));
	if (pIOContext) {
		// pIOContext->Overlapped.Internal = 0;
		// pIOContext->Overlapped.InternalHigh = 0;
		// pIOContext->Overlapped.Offset = 0;
		// pIOContext->Overlapped.OffsetHigh = 0;
		// pIOContext->Overlapped.hEvent = NULL;
		pIOContext->IOOperation = ClientIoAccept;
		pIOContext->nTotalBytes = 0;
		pIOContext->nSentBytes = 0;
		pIOContext->IoContextType.Type.TcpMarker = true;
		// pIOContext->wsabuf.len = 4;
		// pIOContext->wsabuf.buf = NULL;
		pIOContext->InBuffer = NULL; 
		pIOContext->InBufSize = 0; 
		pIOContext->OutBuffer = NULL; 
		pIOContext->OutBufSize = 0;
		pIOContext->pConnection = Connection;
	}
	return pIOContext;
}

//
// Our own printf. This is done because calling printf from multiple
// threads can AV. The standard out for WriteConsole is buffered...
//
int myprintf(const char* lpFormat, ...) {

	// int nLen = 0;
	// int res = 0;
	// char cBuffer[512];
	// va_list arglist;
	// HANDLE hOut = NULL;
	// HRESULT hRet;

	// ZeroMemory(cBuffer, sizeof(cBuffer));

	// va_start(arglist, lpFormat);

	// nLen = lstrlen(lpFormat);
	// hRet = StringCchVPrintf(cBuffer, 512, lpFormat, arglist);

	// if (res >= nLen || GetLastError() == 0) {
	// 	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	// 	if (hOut != INVALID_HANDLE_VALUE)
	// 		WriteConsole(hOut, cBuffer, lstrlen(cBuffer), (LPDWORD)&nLen, NULL);
	// }

	// return nLen;
}




