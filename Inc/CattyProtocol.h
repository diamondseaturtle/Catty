#pragma once
#include <string>
#include <vector>
#include <pair>
#include <memory>
#include <unordered_map>
#include <winsock2.h>
#include <stdio.h>
#include <windows.h>

typedef unsigned long long UserID;
typedef unsigned long long RoomID;

std::unordered_map<RoomID, std::shared_ptr<CattyRoom>> AllRooms;
std::unordered_map<UserID, std::shared_ptr<CattyUser>> AllUsers;

class CattyConnection {
	SOCKET Sock;
	sockaddr Addr;
	int Status; //possible values: active, disconnecting, disconnected
};

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
	SendMessage = 0,
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
	unsigned long long TransactionID; // generate unique id
	Action Command;
	bool IsRequest;

};

class SendMessageReq : MessageHeader {
	UserID ReceiverID // null means send to everyone 
	std::string MessageBody; // utf8 encoded
};

class SendMessageRes : MessageHeader {
	int Status; //0 is ok, 1 is too long, 2 is bogus receiver, 3 is invalid encoding
};

class JoinRoomReq : MessageHeader {
	RoomID RoomToJoin;
	str::string UserName;
};

class JoinRoomRes : MessageHeader {
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

class CreateRoomReq : MessageHeader {
	std::string RoomName;
	unsigned int Capacity;
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
