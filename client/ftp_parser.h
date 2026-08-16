#ifndef FTP_PARSER_H
#define FTP_PARSER_H

#include <winsock2.h>
#include <string>

void runCommandParser(SOCKET control_sock, const std::string& serverIP);
bool sendCommand(SOCKET sock, const std::string& cmd);
std::string receiveReply(SOCKET sock);

#endif
