#pragma once
#ifndef _CATTYPROTOCOL_H_
#define _CATTYPROTOCOL_H_

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <unordered_map>

#include <stdio.h>
#include "../Server/CattyServer.h"

extern ThreadPool pool;

#define SUCCESS 0
#define DECODE_FAILURE 1
#define ROOM_EXISTS_FAILURE 2
#define INSUFFICIENT_RESOURCES 3
#define ENCODE_FAILURE 4
#define SEND_FAILURE 5

typedef unsigned long  UserID;
typedef unsigned long  RoomID;


// class CattyConnection {
// public:
// 	// SOCKET Sock;
// 	int 	 Sock;
// 	PPER_SOCKET_CONTEXT Context;
// 	sockaddr Addr;
// 	int Status; //possible values: active, disconnecting, disconnected
// };

class CattyRoom;

class CattyUser { //destroy when leaving room
public:
	UserID UID; 
	std::string UserName;
	PPER_SOCKET_CONTEXT Connection;
	std::shared_ptr<CattyRoom> CurrentRoom; 
	unsigned int Level;
};

class CattyRoom {
	std::string RoomName;
	std::unordered_map<UserID, std::shared_ptr<CattyUser>> UsersInRoom;
	unsigned int Capacity; 

public: 
	CattyRoom(std::string Name, int Cap) {
		RoomName = Name; 
		Capacity = Cap; 
	}
};


enum Action {
	/// 
	/// User commands
	///
	SendChat = 0,
	JoinRoom, 
	ExitRoom,
	ListRoom, 
	QueryRoom,
	BlockUser,
	Message,

	//
	// Management commands (local only)
	//
	CreateRoom, 
	DeleteRoom,
	KickUser
};

class MessageHeader {
protected: 
	unsigned long TransactionID; // generate unique id
	Action Command;
	bool IsRequest;


public: 
	MessageHeader(Action Comm, unsigned long TransID, bool IsReq) {
		Command = Comm; 
		TransactionID = TransID; 
		IsRequest = IsReq;

	}

	virtual bool IsValidReq() {
		return false;
	}

	virtual MessageHeader* Execute() {
		return nullptr; //def failure
	}

	virtual int GetBufSize() {
		return sizeof(TransactionID) + sizeof(Command) + sizeof(IsRequest);
	}

	virtual int Encode(char* OutBuf, unsigned int OutBufSize);


};

class SendMessageReq : public MessageHeader {
	UserID ReceiverID; // null means send to everyone 
	std::string MessageBody; // utf8 encoded
public: 
	SendMessageReq(char* Buf, UserID Recv, unsigned long TransID) : MessageHeader(SendChat, TransID, true){
		ReceiverID = Recv; 
		MessageBody = Buf; 
	}

	//virtual int Encode(char* OutBuf, unsigned int OutBufSize);

	virtual MessageHeader* Execute();

	virtual int GetBufSize() {
		return MessageHeader::GetBufSize() + sizeof(ReceiverID) + sizeof(unsigned int) + MessageBody.length();
	}
};

class SendMessageRes : MessageHeader {
	int Status; //0 is ok, 1 is too long, 2 is bogus receiver, 3 is invalid encoding
public:
	SendMessageRes(int stat, unsigned long trans_id) : MessageHeader(SendChat, trans_id, false) {
		Status = stat;
	}

	virtual int Encode(char* OutBuf, unsigned int OutBufSize);
	

	virtual int GetBufSize() {
		return MessageHeader::GetBufSize() + sizeof(Status);
	}
};

class JoinRoomReq : public MessageHeader {
	RoomID RoomToJoin;
	std::string UserName;

public: 
	JoinRoomReq(char* Buf, RoomID Destination, unsigned long TransID) : MessageHeader(JoinRoom, TransID, true){
		RoomToJoin = Destination; 
		UserName = Buf;
	}
};

class JoinRoomRes : public MessageHeader {
	int Status; // 0 is ok, 1 is invalid room, 2 is room full
	UserID UID;
};

class ExitRoomReq : MessageHeader {

};

class ExitRoomRes : MessageHeader {
	int Status; //0 is ok
};


class ListRoomReq : MessageHeader {
	
};

class ListRoomRes : MessageHeader {
	int Status;
	std::vector<std::pair<RoomID, std::string>> Rooms;
};

class QueryRoomReq : MessageHeader {
	RoomID Room;
};

class QueryRoomRes : MessageHeader {
	int Status;
	std::shared_ptr<CattyRoom> RoomInfo;

};

class BlockUserReq : MessageHeader {
	UserID UserToBlock;
};

class BlockUserRes : MessageHeader {
	int Status;
};

class CreateRoomReq : public MessageHeader {
	std::string RoomName;
	unsigned int Capacity;

public : 
	CreateRoomReq(char* Buf, unsigned int Cap, unsigned long TransID) : MessageHeader(CreateRoom, TransID, true) {
		Capacity = Cap;
		RoomName = Buf;
	}

	virtual bool IsValidReq();

	virtual MessageHeader* Execute();

	//virtual int Encode(char* OutBuf, unsigned int OutBufSize);

	virtual int GetBufSize() {
		return MessageHeader::GetBufSize() + sizeof(Capacity) + sizeof(unsigned int) + RoomName.length();
	}

	
};

class CreateRoomRes : public MessageHeader {
	int Status; 

public: 
	CreateRoomRes(int Stat, unsigned long TransID) : MessageHeader(CreateRoom, TransID, false) {
		Status = Stat;
	}

	virtual int Encode(char* OutBuf, unsigned int OutBufSize);

	virtual int GetBufSize() {
		return MessageHeader::GetBufSize() + sizeof(Status);
	}
};

class DeleteRoomReq : MessageHeader {
	RoomID RoomToDelete; 
};

class DeleteRoomRes : MessageHeader {
	int Status;
};

class KickUserReq : MessageHeader {
	UserID UserToKick;
};

class KickUserRes : MessageHeader {
	int Status;
};

int EncodeMsg(char* out_buf, char* body, unsigned int buf_size, int body_length);
int SendMsg(char* out_buf, char* body, unsigned int buf_size, int body_length, PPER_SOCKET_CONTEXT connection);

int SendUtil(int fd, unsigned int size, char* buffer);



#endif