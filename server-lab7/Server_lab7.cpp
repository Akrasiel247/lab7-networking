/*
Name: Ruby Anne Bautista
BTN415-Lab 5
*/
#include <windows.networking.sockets.h>
#pragma comment(lib, "Ws2_32.lib")

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include "glm\glm.hpp"
#include "Serialize.h"



#define MAX_CLIENTS 3

using namespace std;



struct Actor
{
	int id;	//Unique ID of this actor
	float x_pos, y_pos;	//Position of a Actor
	bool isAlive;	//Still alive?
	bool isBullet;	//Is this actor a bullet or a player?
	bool touchedActor;	//If this Actor is collided with other Actor
	glm::vec2 direction;	//Motion direction
};



std::vector<Actor> gameScene;

void makeGameScene(Actor& a) {

	//find if it exists already
	auto it = std::find_if(gameScene.begin(), gameScene.end(), [&](Actor c) {return c.id==a.id; });
	//TODO: check std::map

	//if !exists
	if (it == gameScene.end()) {
		gameScene.push_back(a);
	}
	else {//replace/update existing instance
		*(it) = a;
	}

	for (Actor a : gameScene) {
		cout << a.id << endl;
	}

}

void deleteActor(int id) {

	gameScene.erase(std::remove_if(gameScene.begin(), gameScene.end(), [&](Actor a) {return a.id == id; }));

}

void handle_Client(SOCKET s, int id) {

	int flag = 0;
	while (flag != -1) {

		Actor x;
		char* rvbuff = (char*)malloc(sizeof(Actor));

		//receive from client
		int n = recv(s, rvbuff, sizeof(Actor), 0);
		if (n > 0) {

			memcpy(&x, rvbuff, sizeof(Actor));
			makeGameScene(x);



		}
		//Should I make a deleteActor func to handle clients that leave
		else if (n == 0) {
			cout << id << (" Connection closed\n");
			flag = -1;
			deleteActor(x.id);
		}
		else {
			cout << "This is the device" << id << " recv failed: " << WSAGetLastError();
			flag = -1;
			deleteActor(x.id);

		}


		//Serialize

		char* t = convert(gameScene);
		int y = sizeof(Actor) * gameScene.size();

		send(s, t, y, 0);



	}


}





int main()
{

	struct sockaddr_in SvrAddr;				//structure to hold servers IP address-holds param- port num, ip address
	SOCKET WelcomeSocket, ConnectionSocket;	//socket descriptors
	WSADATA wsaData;

	//Data buffers


	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)///What is this doing?
		return -1;

	//create welcoming socket at port and bind local address
	if ((WelcomeSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		return -1;


	SvrAddr.sin_family = AF_INET;			//set family to internet

	SvrAddr.sin_addr.s_addr = INADDR_ANY;   //inet_addr("127.0.0.1");	//set the local IP address
	//To be completed by students
	//What does 127.0.0.1 represent?
	///what is htons?
	SvrAddr.sin_port = htons(27000);							//set the port number- 
	//To be complete by students:
	//Now, try to communicate through port number 27001

	if ((bind(WelcomeSocket, (struct sockaddr*) & SvrAddr, sizeof(SvrAddr))) == SOCKET_ERROR)
	{
		closesocket(WelcomeSocket);
		WSACleanup();
		return -1;
	}

	//Specify the maximum number of clients that can be queued
	if (listen(WelcomeSocket, MAX_CLIENTS) == SOCKET_ERROR)
	{
		closesocket(WelcomeSocket);
		std::cout << "Connection failed- Socket Error!" << std::endl;

		return -1;
	}

	//Main server loop - accept and handle requests from clients
	std::cout << "Waiting for client connection" << std::endl;



	std::thread t[MAX_CLIENTS];

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		//wait for an incoming connection from a client
		if ((ConnectionSocket = (accept(WelcomeSocket, NULL, NULL))) == SOCKET_ERROR)///accept will wait for connection
		{

			closesocket(WelcomeSocket);
			std::cout << "Sorry bad connection it was rejected" << std::endl;

			return -1;
		}
		else
		{
			t[i] = std::thread(handle_Client, ConnectionSocket, i + 1);
			cout << "Thread Created for Client: " << 1 + i << endl;

		}
	}


	for (int i = 0; i < MAX_CLIENTS; ++i) {
		t[i].join();
	}

	closesocket(ConnectionSocket);



	WSACleanup();

	std::cout << "Successful Run\n";
}