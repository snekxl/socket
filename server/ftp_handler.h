#pragma once
#include <string>
#include <map>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

// Sử dụng extern để chia sẻ các biến toàn cục này với server_main.cpp
extern std::mutex mtx;
extern std::map<SOCKET, std::string> connected_clients;

void printClientTable();
void handleClientSession(SOCKET client_sock, std::string client_ip);
