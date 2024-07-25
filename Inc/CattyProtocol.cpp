#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "CattyProtocol.h"
// #include "Catty.h"
#include "ThreadPool.h"
#include "../Server/CattyServer.h"

std::unordered_map<std::string, std::shared_ptr<CattyRoom>> AllRooms;
std::unordered_map<UserID, std::shared_ptr<CattyUser>> AllUsers;
std::unordered_map<int, PPER_SOCKET_CONTEXT> AllConnections;


int SendUtil(int fd, unsigned int size, char* buffer){
    unsigned int left = size; 
    while (left > 0) {
        int res = write(fd,
                    (void*) (buffer + (size - left)), 
                    left);
        if (res == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                return SEND_FAILURE;
            }
            continue;
        } else if (res == 0) {
            break;
        }

        left -= res;
    }

    return SUCCESS;
}

int EncodeMsg(char* out_buf, char* body, unsigned int buf_size, int body_length) {
    MessageHeader header(Message, 0, false);
	int result = header.Encode(out_buf, buf_size);
	if (result == SUCCESS) {
		char* curr = out_buf + HEADER_SIZE;
		unsigned int left = buf_size - HEADER_SIZE;

		if (buf_size >= sizeof(body_length)) {
			*((unsigned int*) curr) = htonl(body_length);
		} else {
			result = ENCODE_FAILURE;
		}

		if (result == SUCCESS) {
			curr += sizeof(body_length);
			left -= sizeof(body_length);

			if (left >= body_length) {
				curr = body;
			}
		}
	}

	return result;
}

int SendMsg(char* out_buf, char* body, unsigned int buf_size, int body_length, PPER_SOCKET_CONTEXT connection) {
    int result = EncodeMsg(out_buf, body, buf_size, body_length);
    if (result == SUCCESS) {
        connection->SendLock.lock();
        result = SendUtil(connection->Socket, buf_size, out_buf);
        connection->SendLock.unlock();
    }
    return result;
}

//--------------------------------- PROTOCOL LIB FUNCTIONS ---------------------------------------//

bool CreateRoomReq::IsValidReq() {
	if (Capacity == 0 || Capacity > 10) {
		return false;
	}
	return true;
}

MessageHeader* CreateRoomReq::Execute() {
	int Status = SUCCESS;
	if (AllRooms.find(RoomName) != AllRooms.end()) {
		Status = ROOM_EXISTS_FAILURE;
	}

	if (Status == SUCCESS) {
		CattyRoom* Room = new CattyRoom(RoomName, Capacity);
		AllRooms.emplace(RoomName, std::move(Room));
	}
	
	CreateRoomRes* Response = new CreateRoomRes(Status, TransactionID);

	return Response;
}

MessageHeader* SendMessageReq::Execute() {
	int status = SUCCESS;
	if (AllUsers.find(ReceiverID) == AllUsers.end()) {
		status = SEND_FAILURE;
	}

	if (status == SUCCESS) {
        unsigned int size = SendMessageReq::GetBufSize();
        char* out_buf = (char*) malloc(size); 

        // should copy body lol
        pool.enqueue(SendMsg, out_buf, &MessageBody[0], 
                        size, MessageBody.length(), 
                        AllUsers[ReceiverID]->Connection);
	}

	return nullptr; //Change
}

int MessageHeader::Encode(char* OutBuf, unsigned int OutBufSize) {
	int Result = SUCCESS;
	char* CurrentPtr = OutBuf; 
	unsigned int Size = OutBufSize; 
	if (Size >= 4) {
		*((unsigned int*) CurrentPtr) = htonl(MAGIC); 
	}
	else {
		Result = ENCODE_FAILURE;
	}

	if (Result == SUCCESS) {
		CurrentPtr += sizeof(MAGIC); 
		Size -= sizeof(MAGIC); 
		if (Size >= sizeof(TransactionID)) {
			*((unsigned long*) CurrentPtr) = htonl(TransactionID); 
		}
		else {
			Result = ENCODE_FAILURE;
		}
	}

	if (Result == SUCCESS) {
		CurrentPtr += sizeof(TransactionID); 
		Size -= sizeof(TransactionID); 
		if (Size >= sizeof(Command)) { 
			*((unsigned int*) CurrentPtr) = htonl(Command);
		}
		else {
			Result = ENCODE_FAILURE;
		}
	}

	if (Result == SUCCESS) {
		CurrentPtr += sizeof(Command); 
		Size -= sizeof(Command); 
		if (Size >= sizeof(IsRequest)) {
			*((bool*)CurrentPtr) = htonl(IsRequest); 
		}
		else {
			Result = ENCODE_FAILURE;
		}
	}
	return Result;
}


int CreateRoomRes::Encode(char* OutBuf, unsigned int OutBufSize) {
	int Result = MessageHeader::Encode(OutBuf, OutBufSize);
	if (Result == SUCCESS) {
		char* CurrentPtr = OutBuf + HEADER_SIZE; // this or return position in super encode
		unsigned int Size = OutBufSize - HEADER_SIZE; 
		if (Size >= sizeof(Status)) {
			*((unsigned int*)CurrentPtr) = htonl(Status); 
		}
		else {
			Result = ENCODE_FAILURE;
		}
	}

	return Result;
	
}