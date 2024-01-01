#pragma warning (disable:4127)

#ifdef _IA64_
	#pragma warning(disable:4706 4267)
#endif 

#include <stdlib.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <strsafe.h>

#include "..\Inc\CattyProtocol.h"
#include "..\Inc\Catty.h"

#define MAXTHREADS 64

#define xmalloc(s) HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(s))
#define xfree(p)   {HeapFree(GetProcessHeap(),0,(p)); p = NULL;}

typedef struct _OPTIONS {
	char szHostname[64];
	char *port;
	unsigned int nTotalThreads;
	int nBufSize;
	BOOL bVerbose;
} OPTIONS;

typedef struct THREADINFO {
	HANDLE hThread[MAXTHREADS];
	SOCKET sd;
} THREADINFO;

static OPTIONS default_options = {"localhost", "5001", 1, 4096, FALSE};
static OPTIONS g_Options;
static THREADINFO g_ThreadInfo;
static BOOL g_bEndClient = FALSE;
static WSAEVENT g_hCleanupEvent[1];
HANDLE g_hIOCP = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_CriticalSection;

static BOOL WINAPI CtrlHandler (DWORD dwEvent);
static BOOL ValidOptions(char *argv[], int argc);
static VOID Usage(char *szProgramname, OPTIONS *pOptions);
static DWORD WINAPI EchoThread(LPVOID lpParameter);
static BOOL CreateConnectedSocket();
static BOOL SendBuffer(int nThreadNum, char *outbuf);
static BOOL RecvBuffer(int nThreadNum, char *inbuf);
static int myprintf(const char *lpFormat, ...);

DWORD WINAPI WorkerThread(LPVOID WorkThreadContext) {

	HANDLE hIOCP = (HANDLE)WorkThreadContext;
	BOOL bSuccess = FALSE;
	int nRet = 0;
	LPWSAOVERLAPPED lpOverlapped = NULL;
	PPER_SOCKET_CONTEXT lpPerSocketContext = NULL;
	PPER_IO_CONTEXT lpIOContext = NULL;
	WSABUF buffRecv;
	WSABUF buffSend;
	DWORD dwRecvNumBytes = 0;
	DWORD dwSendNumBytes = 0;
	DWORD dwFlags = 0;
	DWORD dwIoSize = 0;

	while (TRUE) {

		//
		// continually loop to service io completion packets
		//
		bSuccess = GetQueuedCompletionStatus(hIOCP, &dwIoSize,
			(PDWORD_PTR)&lpPerSocketContext,
			(LPOVERLAPPED*)&lpOverlapped,
			INFINITE);
		if (!bSuccess)
			myprintf("GetQueuedCompletionStatus() failed: %d\n", GetLastError());

		if (lpPerSocketContext == NULL) {

			//
			// CTRL-C handler used PostQueuedCompletionStatus to post an I/O packet with
			// a NULL CompletionKey (or if we get one for any reason).  It is time to exit.
			//
			return(0);
		}

		if (g_bEndClient) {

			//
			// main thread will do all cleanup needed - see finally block
			//
			return(0);
		}

		if (!bSuccess || (bSuccess && (dwIoSize == 0))) {

			//
			// client connection dropped, continue to service remaining (and possibly 
			// new) client connections
			//
			//CloseClient(lpPerSocketContext, FALSE);
			continue;
		}

		//
		// determine what type of IO packet has completed by checking the PER_IO_CONTEXT 
		// associated with this socket.  This will determine what action to take.
		//
		lpIOContext = (PPER_IO_CONTEXT)lpOverlapped;
		switch (lpIOContext->IOOperation) {
		case ClientIoRead:
			// is this tcpsize?
			// if entire message received then 
			//		decode
			//		if this a response
			//			build response object; 
			//			pIOContext  = find and remove the request from the req pending queue by txid
			//			put the resposne to pIoContext
			//			if (pIoContext->Callback) {
			//				(*pIoContext->Callback)(pIoContext->CallbackContext);
			//			}
			//	submit a ClinetIoRead for TCP Size. 
					
			break;

		case ClientIoWrite:
			break;
				
		} //switch
	} //while
	return(0);
}

int __cdecl main(int argc, char *argv[]) {

	OSVERSIONINFO verInfo = {0};
	WSADATA WSAData; 
	DWORD dwThreadId = 0;
	DWORD dwRet = 0;
	BOOL bInitError = FALSE;
	int nThreadNum[MAXTHREADS];
	int i = 0;
	int nRet = 0;

	verInfo.dwOSVersionInfoSize = sizeof(verInfo);
	GetVersionEx(&verInfo);
	if( verInfo.dwPlatformId  == VER_PLATFORM_WIN32_WINDOWS ) {

		//
		// Since this application can heavily stress system resources
		// we decided to limit running it on NT.
		//
		myprintf("Please run %s only on NT, thank you\n", argv[0]);
		return(0);
	}

	for( i = 0; i < MAXTHREADS; i++ ) {
		g_ThreadInfo.sd = INVALID_SOCKET;
		g_ThreadInfo.hThread[i] = INVALID_HANDLE_VALUE;
		nThreadNum[i] = 0;
	}

	g_hCleanupEvent[0] = WSA_INVALID_EVENT;

	if( !ValidOptions(argv, argc) )
		return(1);

	if( (nRet = WSAStartup(MAKEWORD(2,2), &WSAData)) != 0 ) {
		myprintf("WSAStartup() failed: %d", nRet);
		return(1);
	}

	if(WSA_INVALID_EVENT == (g_hCleanupEvent[0] = WSACreateEvent()))
	{
		myprintf("WSACreateEvent() failed: %d\n", WSAGetLastError());
		WSACleanup();
		return(1);
	}

	//
	// be able to gracefully handle CTRL-C and close handles
	//
	if( !SetConsoleCtrlHandler (CtrlHandler, TRUE) ) {
		myprintf("SetConsoleCtrlHandler() failed: %d\n", GetLastError());
		if( g_hCleanupEvent[0] != WSA_INVALID_EVENT ) {
			WSACloseEvent(g_hCleanupEvent[0]);
			g_hCleanupEvent[0] = WSA_INVALID_EVENT;
		}
		WSACleanup();
		return(1);
	}

	//
	// spawn the threads 
	//


		//
		// if CTRL-C is pressed before all the sockets have connected, closure of
		// the program could take a little while, especially if the server is 
		// down and we have to wait for connect to fail.  Checking for this
		// global flag allows us to shortcircuit that.
		//
	
	if( CreateConnectedSocket())  {

		//
		// a unique memory location needs to be passed into each thread, 
		// otherwise the value would change by the time all the threads 
		// get a chance to run.
		//

		g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (g_hIOCP == NULL) {
			myprintf("CreateIoCompletionPort() failed to create I/O completion port: %d\n",
				GetLastError());
		}

		for (DWORD dwCPU = 0; dwCPU < g_Options.nTotalThreads; dwCPU++) {

			//
			// Create worker threads to service the overlapped I/O requests.  The decision
			// to create 2 worker threads per CPU in the system is a heuristic.  Also,
			// note that thread handles are closed right away, because we will not need them
			// and the worker threads will continue to execute.
			//
			HANDLE hThread = INVALID_HANDLE_VALUE;
			DWORD dwThreadId = 0;

			hThread = CreateThread(NULL, 0, WorkerThread, g_hIOCP, 0, &dwThreadId);
			if (hThread == NULL) {
				myprintf("CreateThread() failed to create worker thread: %d\n",
					GetLastError());
				//todo: exit
			}
			g_ThreadInfo.hThread[dwCPU] = hThread;
			hThread = INVALID_HANDLE_VALUE;
		}

		// create IOContxt for receive -> associate the socket with IOCP
		// submit a receive IO -> <ClientIoRead>

		/*nThreadNum[i] = i;
		g_ThreadInfo.hThread[i] = CreateThread(NULL, 0, EchoThread, (LPVOID)&nThreadNum[i], 0, &dwThreadId);
		if( g_ThreadInfo.hThread[i] == NULL ) {
			myprintf("CreateThread(%d) failed: %d\n", i, GetLastError());
			bInitError = TRUE;
				
		}*/
	}
	
	while (true) {
		// read user input + parse + send request + process response
	}

    if( !bInitError ) {

		//
		// wait for the threads to exit
		//
		dwRet = WaitForMultipleObjects(g_Options.nTotalThreads, g_ThreadInfo.hThread, TRUE, INFINITE);
		if( dwRet == WAIT_FAILED )
			myprintf("WaitForMultipleObject(): %d\n", GetLastError());
	}

    if( !GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0) ) {
		myprintf("GenerateConsoleCtrlEvent() failed: %d\n", GetLastError());
	};

    if( WSAWaitForMultipleEvents(1, g_hCleanupEvent, TRUE, WSA_INFINITE, FALSE) == WSA_WAIT_FAILED ) {
		myprintf("WSAWaitForMultipleEvents() failed: %d\n", WSAGetLastError());
	};

    if( g_hCleanupEvent[0] != WSA_INVALID_EVENT ) {
		WSACloseEvent(g_hCleanupEvent[0]);
		g_hCleanupEvent[0] = WSA_INVALID_EVENT;
	}

	WSACleanup();

	//
	// Restores default processing of CTRL signals.
	//
	SetConsoleCtrlHandler(CtrlHandler, FALSE);
	SetConsoleCtrlHandler(NULL, FALSE);

	return(0);
}

PPER_SOCKET_CONTEXT CtxtAllocate(SOCKET sd) {

	PPER_SOCKET_CONTEXT lpPerSocketContext;

	__try
	{
		EnterCriticalSection(&g_CriticalSection);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		myprintf("EnterCriticalSection raised an exception.\n");
		return NULL;
	}

	lpPerSocketContext = (PPER_SOCKET_CONTEXT)xmalloc(sizeof(PER_SOCKET_CONTEXT));
	if (lpPerSocketContext) {
		lpPerSocketContext->Socket = sd;
		lpPerSocketContext->pCtxtBack = NULL;
		lpPerSocketContext->pCtxtForward = NULL;
	}
	else {
		myprintf("HeapAlloc() PER_SOCKET_CONTEXT failed: %d\n", GetLastError());
	}

	LeaveCriticalSection(&g_CriticalSection);

	return(lpPerSocketContext);
}

PPER_SOCKET_CONTEXT UpdateCompletionPort(SOCKET sd, BOOL bAddToList) {

	PPER_SOCKET_CONTEXT lpPerSocketContext;


	lpPerSocketContext = CtxtAllocate(sd);
	if (lpPerSocketContext == NULL)
		return(NULL);

	g_hIOCP = CreateIoCompletionPort((HANDLE)sd, g_hIOCP, (DWORD_PTR)lpPerSocketContext, 0);
	if (g_hIOCP == NULL) {
		myprintf("CreateIoCompletionPort() failed: %d\n", GetLastError());
		if (lpPerSocketContext->pIOContext)
			xfree(lpPerSocketContext->pIOContext);
		xfree(lpPerSocketContext);
		return(NULL);
	}

	//
	//The listening socket context (bAddToList is FALSE) is not added to the list.
	//All other socket contexts are added to the list.
	//
	/*if (bAddToList) CtxtListAddTo(lpPerSocketContext);

	if (g_bVerbose)
		myprintf("UpdateCompletionPort: Socket(%d) added to IOCP\n", lpPerSocketContext->Socket);
	*/
	return(lpPerSocketContext);
}

void ReceiveResponseCallback(PVOID Context) {
	PHANDLE hEvent = (PHANDLE) Context;
	SetEvent(*hEvent);
}

int SendReqWaitReply(SOCKET Sock, MessageHeader* Request, MessageHeader* &Response) {
	int Result = SUCCESS;
	PPER_SOCKET_CONTEXT lpPerSocketContext = UpdateCompletionPort(Sock, TRUE);
	PPER_IO_CONTEXT pIOContext;
	HANDLE hEvent;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == INVALID_HANDLE_VALUE) {
		Result = INSUFFICIENT_RESOURCES;
	}
	
	if (Result == SUCCESS) {
		lpPerSocketContext = UpdateCompletionPort(Sock, TRUE);
		if (lpPerSocketContext == NULL) {
			Result = INSUFFICIENT_RESOURCES;
		}
	}

	if (Result == SUCCESS) {
		pIOContext = AllocIOContext(lpPerSocketContext);
		if (pIOContext == NULL) {
			Result = INSUFFICIENT_RESOURCES;
			//todo: delete lpPerSock
		}
	}
	
	if (Result == SUCCESS) {
		pIOContext->Request = Request; 
		pIOContext->Callback = ReceiveResponseCallback;
		pIOContext->CallbackContext = &hEvent;
		//todo: getbuf size, alloc buff, encode request, send
		//todo: add request into global queue before IO
	}

	if (Result == SUCCESS) {
		if (WAIT_OBJECT_0 != WaitForSingleObject(hEvent, 5 * MS)) {
			Result = SEND_FAILURE;
		}
	}
	if (Result == SUCCESS) {
		Response = pIOContext->Response;
	}
	// todo: close hEvent, delete IoCtxt 
	return Result;
}

PPER_IO_CONTEXT AllocIOContext(PPER_SOCKET_CONTEXT Connection) {
	PPER_IO_CONTEXT pIOContext = (PPER_IO_CONTEXT)xmalloc(sizeof(PER_IO_CONTEXT));
	if (pIOContext) {
		pIOContext->Overlapped.Internal = 0;
		pIOContext->Overlapped.InternalHigh = 0;
		pIOContext->Overlapped.Offset = 0;
		pIOContext->Overlapped.OffsetHigh = 0;
		pIOContext->Overlapped.hEvent = NULL;
		pIOContext->IOOperation = ClientIoAccept;
		pIOContext->pIOContextForward = NULL;
		pIOContext->nTotalBytes = 0;
		pIOContext->nSentBytes = 0;
		pIOContext->IoContextType.Type.TcpMarker = true;
		pIOContext->wsabuf.len = 4;
		pIOContext->wsabuf.buf = NULL;
		pIOContext->InBuffer = NULL;
		pIOContext->InBufSize = 0;
		pIOContext->OutBuffer = NULL;
		pIOContext->OutBufSize = 0;
		pIOContext->pConnection = Connection;
	}
	return pIOContext;
}

//
// Abstract:
//     This is the thread that continually sends and receives a specific size
//     buffer to the server.  Upon receipt of the echo from the server, a
//     simple check is performed to check the integrity of the transfer.
//
static DWORD WINAPI EchoThread(LPVOID lpParameter) {

	char *inbuf  = NULL;
	char *outbuf = NULL;
	int *pArg = (int *)lpParameter;
	int nThreadNum = *pArg;

	myprintf("Starting thread %d\n", nThreadNum);

    inbuf = (char *)xmalloc(g_Options.nBufSize);
    outbuf = (char *)xmalloc(g_Options.nBufSize);

	if( (inbuf) && (outbuf)) {

		//
		// NOTE data possible data loss with INT conversion to BYTE
		//
		FillMemory(outbuf, g_Options.nBufSize, (BYTE)nThreadNum);

		while( TRUE ) {

			//
			// just continually send and wait for the server to echo the data
			// back.  Just do a simple minded comparison.
			//
			if( SendBuffer(nThreadNum, outbuf) && 
				RecvBuffer(nThreadNum, inbuf) ) {
				if( (inbuf[0] == outbuf[0]) && 
					(inbuf[g_Options.nBufSize-1] == outbuf[g_Options.nBufSize-1]) ) {
					if( g_Options.bVerbose )
						myprintf("ack(%d)\n", nThreadNum);
				} else {
					myprintf("nak(%d) in[0]=%d, out[0]=%d in[%d]=%d out[%d]%d\n", 
							 nThreadNum,
							 inbuf[0], outbuf[0], 
							 g_Options.nBufSize-1, inbuf[g_Options.nBufSize-1], 
							 g_Options.nBufSize-1, outbuf[g_Options.nBufSize-1]);
					break;
				}
			} else
				break;
		}
	}

	if( inbuf )
		xfree(inbuf);
	if( outbuf )
		xfree(outbuf);

	return(TRUE);
}

//
// Abstract:
//     Create a socket and connect to the server process.
//
static BOOL CreateConnectedSocket() {

	BOOL bRet = TRUE;
	int nRet = 0;
	struct addrinfo hints = {0};
	struct addrinfo *addr_srv = NULL;

	//
	// Resolve the interface
	//
    hints.ai_flags  = 0;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if( getaddrinfo(g_Options.szHostname, g_Options.port, &hints, &addr_srv) != 0 ) {
		myprintf("getaddrinfo() failed with error %d\n", WSAGetLastError());
		bRet = FALSE;
	}

	if( addr_srv == NULL ) {
		myprintf("getaddrinfo() failed to resolve/convert the interface\n");
		bRet = FALSE;
	} else {
		g_ThreadInfo.sd = socket(addr_srv->ai_family, addr_srv->ai_socktype, addr_srv->ai_protocol);
		if( g_ThreadInfo.sd == INVALID_SOCKET ) {
			myprintf("socket() failed: %d\n", WSAGetLastError());
			bRet = FALSE;
		}
	}

	if( bRet != FALSE ) {
		nRet = connect(g_ThreadInfo.sd, addr_srv->ai_addr, (int) addr_srv->ai_addrlen);
		if( nRet == SOCKET_ERROR ) {
			myprintf("connect(thread ) failed: %d\n", WSAGetLastError());
			bRet = FALSE;
		} else
			myprintf("connected(thread)\n");

		freeaddrinfo(addr_srv);
	}

	return(bRet);
}

//
// Abstract:
//     Send a buffer - keep send'ing until the requested amount of
//     data has been sent or the socket has been closed or error.
//
static BOOL SendBuffer(int nThreadNum, char *outbuf) {

	BOOL bRet = TRUE;
	char *bufp = outbuf;
	int nTotalSend = 0;
	int nSend = 0;

	while( nTotalSend < g_Options.nBufSize ) {
		nSend = send(g_ThreadInfo.sd, bufp, g_Options.nBufSize - nTotalSend, 0);
		if( nSend == SOCKET_ERROR ) {
			myprintf("send(thread=%d) failed: %d\n", nThreadNum, WSAGetLastError());
			bRet = FALSE;
			break;
		} else if( nSend == 0 ) {
			myprintf("connection closed\n");
			bRet = FALSE;
			break;
		} else {
			nTotalSend += nSend;
			bufp += nSend;
		}
	}

	return(bRet);
}

//
// Abstract:
//     Receive a buffer - keep recv'ing until the requested amount of
//     data has been received or the socket has been closed or error.
//
static BOOL RecvBuffer(int nThreadNum, char *inbuf) {

	BOOL bRet = TRUE;
	char *bufp = inbuf;
	int nTotalRecv = 0;
	int nRecv = 0;

	while( nTotalRecv < g_Options.nBufSize ) {
		nRecv = recv(g_ThreadInfo.sd, bufp, g_Options.nBufSize - nTotalRecv, 0);
		if( nRecv == SOCKET_ERROR ) {
			myprintf("recv(thread=%d) failed: %d\n", nThreadNum, WSAGetLastError());
			bRet = FALSE;
			break;
		} else if( nRecv == 0 ) {
			myprintf("connection closed\n");
			bRet = FALSE;
			break;
		} else {
			nTotalRecv += nRecv;
			bufp += nRecv;
		}
	}

	return(bRet);
}

//
// Abstract:
//      Verify options passed in and set options structure accordingly.
//
static BOOL ValidOptions(char *argv[], int argc) {

	g_Options = default_options;
    HRESULT hRet;
    
	for( int i=1; i<argc; i++ ) {
		if( (argv[i][0] == '-') || (argv[i][0] == '/') ) {
			switch( tolower(argv[i][1]) ) {
			case 'b' :
				if( lstrlen(argv[i]) > 3 )
					g_Options.nBufSize = 1024*atoi(&argv[i][3]);
				break;

			case 'e' :
				if( lstrlen(argv[i]) > 3 )
					g_Options.port = &argv[i][3];
				break;

			case 'n' : 
				if( lstrlen(argv[i]) > 3 )
                {
                    hRet = StringCbCopyN(g_Options.szHostname,64,&argv[i][3],64);
                }
                break;

			case 't' :
				if( lstrlen(argv[i]) > 3 )
					g_Options.nTotalThreads = min(MAXTHREADS, atoi(&argv[i][3]));
				break;

			case 'v' :
				g_Options.bVerbose = TRUE;
				break;

			case '?' :
				Usage(argv[0], &default_options);
				return(FALSE);
				break;

			default:
				myprintf("  unknown options flag %s\n", argv[i]);
				Usage(argv[0], &default_options);
				return(FALSE);
				break;
			}
		} else {
			myprintf("  unknown option %s\n", argv[i]);
			Usage(argv[0], &default_options);
			return(FALSE);
		}
	}

	return(TRUE);
}

//
// Abstract:
//      Print out usage table for the program
//
static VOID Usage(char *szProgramname, OPTIONS *pOptions) {

	myprintf("usage:\n%s [-b:#] [-e:#] [-n:host] [-t:#] [-v]\n",
			 szProgramname);
	myprintf("%s -?\n", szProgramname);
	myprintf("  -?\t\tDisplay this help\n");
	myprintf("  -b:bufsize\tSize of send/recv buffer; in 1K increments (Def:%d)\n",
			 pOptions->nBufSize);
	myprintf("  -e:port\tEndpoint number (port) to use (Def:%d)\n",
			 pOptions->port);
	myprintf("  -n:host\tAct as the client and connect to 'host' (Def:%s)\n",
			 pOptions->szHostname);
	myprintf("  -t:#\tNumber of threads to use\n");
	myprintf("  -v\t\tVerbose, print an ack when echo received and verified\n");
	return;
}

static BOOL WINAPI CtrlHandler (DWORD dwEvent) {

	int i = 0;
	DWORD dwRet = 0;

	switch( dwEvent ) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_CLOSE_EVENT:

		myprintf("Closing handles and sockets\n");

		//
		// Temporarily disables processing of CTRL_C_EVENT signal.
		//
		SetConsoleCtrlHandler(NULL, TRUE);

		g_bEndClient = TRUE;

		for( i = 0; i < g_Options.nTotalThreads; i++ ) {
			if( g_ThreadInfo.sd != INVALID_SOCKET ) {

				//
				// force the subsequent closesocket to be abortative.
				//
				LINGER  lingerStruct;

				lingerStruct.l_onoff = 1;
				lingerStruct.l_linger = 0;
				setsockopt(g_ThreadInfo.sd, SOL_SOCKET, SO_LINGER,
						   (char *)&lingerStruct, sizeof(lingerStruct));
				closesocket(g_ThreadInfo.sd);
				g_ThreadInfo.sd = INVALID_SOCKET;

				if( g_ThreadInfo.hThread[i] != INVALID_HANDLE_VALUE ) {

					dwRet = WaitForSingleObject(g_ThreadInfo.hThread[i], INFINITE);
					if( dwRet == WAIT_FAILED )
						myprintf("WaitForSingleObject(): %d\n", GetLastError());

					CloseHandle(g_ThreadInfo.hThread[i]);
					g_ThreadInfo.hThread[i] = INVALID_HANDLE_VALUE;
				}
			}
		}

		break;

	default:

		//
		// unknown type--better pass it on.
		//
		return(FALSE);
	}

	WSASetEvent(g_hCleanupEvent[0]);

	return(TRUE);
}

static int myprintf (const char *lpFormat, ... ) {

	int nLen = 0;
	int nRet = 0;
	char cBuffer[512] = {'\0'};
	va_list arglist ;
	HANDLE hOut = NULL;
    HRESULT hRet;
    
	ZeroMemory(cBuffer, sizeof(cBuffer));

	va_start(arglist, lpFormat);

	nLen = lstrlen( lpFormat ) ;

    hRet = StringCchVPrintf(cBuffer,512,lpFormat,arglist);
    
	if( nRet >= nLen || GetLastError() == 0 ) {
		hOut = GetStdHandle(STD_OUTPUT_HANDLE) ;
		if( hOut != INVALID_HANDLE_VALUE )
			WriteConsole( hOut, cBuffer, lstrlen(cBuffer), (LPDWORD)&nLen, NULL ) ;
	}

	return nLen ;
}



