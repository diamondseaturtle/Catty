#pragma once

#include <mswsock.h>


#define DEFAULT_PORT  "5001";
#define MAX_BUFF_SIZE       8192
#define MAX_WORKER_THREAD   16
#define KB 1024
#define MB (1024 * KB)
#define MAGIC 0x3987abcd
#define MS (1000)

#define FAILURE_RESPONSE_SIZE 20 //magic number, messageheader, status
#define HEADER_SIZE 16 //magic, messageheader

typedef enum _IO_OPERATION {
    ClientIoAccept,
    ClientIoRead,
    ClientIoWrite
} IO_OPERATION, * PIO_OPERATION;

struct _PER_SOCKET_CONTEXT;

typedef void (*IOCompletionCallback) (PVOID Context);

//
// data to be associated for every I/O operation on a socket
//
typedef struct _PER_IO_CONTEXT {
    WSAOVERLAPPED               Overlapped;

    WSABUF                      wsabuf;
    int                         nTotalBytes;
    int                         nSentBytes;
    IO_OPERATION                IOOperation;
    //SOCKET                      SocketAccept;
    union {
        struct {
            bool    TcpMarker : 1;
            bool Encoded : 1; 
            bool Decoded : 1;
        } Type;
        unsigned int    Flags;
    } IoContextType;

    struct _PER_IO_CONTEXT* pIOContextForward;
    struct _PER_SOCKET_CONTEXT* pConnection; //connection
    unsigned int                InBufSize;
    unsigned int OutBufSize;
    char* InBuffer;
    char* OutBuffer;
    char ScratchBuffer[FAILURE_RESPONSE_SIZE];
    MessageHeader* Request;
    MessageHeader* Response;
    IOCompletionCallback Callback;
    PVOID CallbackContext;
} PER_IO_CONTEXT, * PPER_IO_CONTEXT;

//
// For AcceptEx, the IOCP key is the PER_SOCKET_CONTEXT for the listening socket,
// so we need to another field SocketAccept in PER_IO_CONTEXT. When the outstanding
// AcceptEx completes, this field is our connection socket handle.
//

//
// data to be associated with every socket added to the IOCP
//
typedef struct _PER_SOCKET_CONTEXT {
    SOCKET                      Socket;

    //LPFN_ACCEPTEX               fnAcceptEx;

    //
    //linked list for all outstanding i/o on the socket
    //
    PPER_IO_CONTEXT             pIOContext;
    sockaddr LocalAddr;
    sockaddr RemoteAddr;
    int Status;
    struct _PER_SOCKET_CONTEXT* pCtxtBack;
    struct _PER_SOCKET_CONTEXT* pCtxtForward;
} PER_SOCKET_CONTEXT, * PPER_SOCKET_CONTEXT;

BOOL ValidOptions(int argc, char* argv[]);

BOOL WINAPI CtrlHandler(
    DWORD dwEvent
);

//BOOL CreateListenSocket(void);

/*BOOL CreateAcceptSocket(
    BOOL fUpdateIOCP
);*/

DWORD WINAPI WorkerThread(
    LPVOID WorkContext
);


PPER_SOCKET_CONTEXT UpdateCompletionPort(
    SOCKET s,
    BOOL bAddToList
);
//
// bAddToList is FALSE for listening socket, and TRUE for connection sockets.
// As we maintain the context for listening socket in a global structure, we
// don't need to add it to the list.
//

PPER_IO_CONTEXT AllocIOContext(PPER_SOCKET_CONTEXT Connection);

/*
VOID CloseClient(
    PPER_SOCKET_CONTEXT lpPerSocketContext,
    BOOL bGraceful
);
*/

PPER_SOCKET_CONTEXT CtxtAllocate(
    SOCKET s
);
/*
VOID CtxtListFree(
);

VOID CtxtListAddTo(
    PPER_SOCKET_CONTEXT lpPerSocketContext
);

VOID CtxtListDeleteFrom(
    PPER_SOCKET_CONTEXT lpPerSocketContext
);
*/

