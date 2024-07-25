CattyServer: CattyServer.o CattyProtocol.o 
	g++ -Wall -g -std=c++17 -pthread -o CattyServer CattyServer.o CattyProtocol.o 

CattyServer.o: ./Server/CattyServer.cpp ./Server/CattyServer.h ./Inc/CattyProtocol.h ./Inc/ThreadPool.h
	g++ -Wall -g -std=c++17 -c ./Server/CattyServer.cpp

CattyProtocol.o: ./Inc/CattyProtocol.cpp ./Inc/CattyProtocol.h ./Server/CattyServer.h ./Inc/ThreadPool.h
	g++ -Wall -g -std=c++17 -c ./Inc/CattyProtocol.cpp
