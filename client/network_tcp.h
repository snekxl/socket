#ifndef NETWORK_TCP_H
#define NETWORK_TCP_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

bool initWinsock();
SOCKET createTCPSocket();
bool connectToServer(SOCKET sock, const std::string& serverIP, int port);
void closeTCPSocket(SOCKET sock);

#endif
