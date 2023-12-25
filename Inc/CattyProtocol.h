#pragma once
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <unordered_map>
#include <winsock2.h>
#include <stdio.h>
#include <windows.h>

typedef unsigned long  UserID;
typedef unsigned long  RoomID;


class CattyConnection {
	SOCKET Sock;
	sockaddr Addr;
	int Status; //possible values: active, disconnecting, disconnected
};

class CattyRoom;

class CattyUser { //destroy when leaving room
	UserID UID; 
	std::string UserName;
	std::shared_ptr<CattyConnection> Connection;
	std::shared_ptr<CattyRoom> CurrentRoom; 
	unsigned int Level;
};

class CattyRoom {
	RoomID RID;
	std::string RoomName;
	std::unordered_map<UserID, std::shared_ptr<CattyUser>> UsersInRoom;
	unsigned int Capacity; 
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

};

class SendMessageReq : public MessageHeader {
	UserID ReceiverID; // null means send to everyone 
	std::string MessageBody; // utf8 encoded
public: 
	SendMessageReq(char* Buf, UserID Recv, unsigned long TransID) : MessageHeader(SendChat, TransID, true){
		ReceiverID = Recv; 
		MessageBody = Buf; 
	}
};

class SendMessageRes : MessageHeader {
	int Status; //0 is ok, 1 is too long, 2 is bogus receiver, 3 is invalid encoding
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
};

class CreateRoomRes : MessageHeader {
	int Status; 
	RoomID CreatedRoom;
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

std::unordered_map<RoomID, std::shared_ptr<CattyRoom>> AllRooms;
std::unordered_map<UserID, std::shared_ptr<CattyUser>> AllUsers;
std::unordered_map<SOCKET, CattyConnection> AllConnections;