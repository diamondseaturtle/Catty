// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (C) 1998 - 2000  Microsoft Corporation.  All Rights Reserved.
//
// Module:
//      iocpserver.h
//

#pragma once
#ifndef _CATTYSERVER_H_
#define _CATTYSERVER_H_

#include <sys/socket.h>
#include <mutex>
#include "../Inc/ThreadPool.h"

#define DEFAULT_PORT  "5001"
#define MAX_BUFF_SIZE       8192
#define MAX_WORKER_THREAD   64
#define KB 1024
#define MB (1024 * KB)
#define MAGIC 0x3987abcd

#define FAILURE_RESPONSE_SIZE 20 //magic number, messageheader, status
#define HEADER_SIZE 16 //magic, messageheader


class MessageHeader;
// class CattyRoom;
// class CattyConnection;
// class CattyUser;

// extern std::unordered_map<std::string, std::shared_ptr<CattyRoom>> AllRooms;
// extern std::unordered_map<unsigned long, std::shared_ptr<CattyUser>> AllUsers;




typedef enum _IO_OPERATION {
    ClientIoAccept,
    ClientIoRead,
    ClientIoWrite
} IO_OPERATION, * PIO_OPERATION;

struct _PER_SOCKET_CONTEXT;

//
// data to be associated for every I/O operation on a socket
//
typedef struct _PER_IO_CONTEXT {
    int                         nTotalBytes;
    int                         nSentBytes;
    IO_OPERATION                IOOperation;

    union {
        struct {
            bool    TcpMarker : 1;
            bool    Encoded : 1; 
            bool    Decoded : 1;
        } Type;
        unsigned int    Flags;
    } IoContextType;

    struct _PER_SOCKET_CONTEXT* pConnection; //assoced socket context
    unsigned int                InBufSize;
    unsigned int                OutBufSize;
    char* InBuffer;
    char* OutBuffer;
    char ScratchBuffer[FAILURE_RESPONSE_SIZE];
    MessageHeader* Request;
    MessageHeader* Response;
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
    int                         Socket;
    bool                        IOActive;
    std::mutex                  SendLock;

    //
    //linked list for all outstanding i/o on the socket
    //
    PPER_IO_CONTEXT             pIOContext;
    sockaddr LocalAddr;
    sockaddr RemoteAddr;
    int Status;
} PER_SOCKET_CONTEXT, * PPER_SOCKET_CONTEXT;

bool ValidOptions(int argc, char* argv[]);

bool CtrlHandler(
    unsigned int dwEvent
);

bool CreateListenSocket(void);

bool CreateAcceptSocket(
    bool fUpdateIOCP
);

unsigned int WorkerThread(
    void* WorkContext
);

PPER_SOCKET_CONTEXT UpdateCompletionPort(
    // SOCKET s,
    int       s,
    bool bAddToList
);
//
// bAddToList is FALSE for listening socket, and TRUE for connection sockets.
// As we maintain the context for listening socket in a global structure, we
// don't need to add it to the list.
//

PPER_IO_CONTEXT AllocIOContext(PPER_SOCKET_CONTEXT Connection);

void CloseClient(
    PPER_SOCKET_CONTEXT lpPerSocketContext,
    bool bGraceful
);

PPER_SOCKET_CONTEXT CtxtAllocate(
    // SOCKET s
    int       s
);

int DecodeIOContext(PPER_IO_CONTEXT pIOContext);

int ProcessIOContext(PPER_IO_CONTEXT pIOContext);

int SendGeneralFailureResponse(PPER_IO_CONTEXT pIOContext, int Result);

#endif