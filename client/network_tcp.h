#pragma once
#include <winsock2.h>
#include <string>

SOCKET connectTCP(const std::string& ip, int port);
std::string receiveTCP(SOCKET sock);
void cleanupTCP(SOCKET sock);