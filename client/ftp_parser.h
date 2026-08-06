#ifndef FTP_PARSER_H
#define FTP_PARSER_H

#include <winsock2.h>
#include <string>

// Chạy vòng lặp nhập lệnh từ bàn phím, truyền thêm serverIP cho luồng UDP
void runCommandParser(SOCKET control_sock, const std::string& serverIP);

// Hàm phụ trợ gửi lệnh
bool sendCommand(SOCKET sock, const std::string& cmd);

// Hàm phụ trợ nhận phản hồi từ Server
std::string receiveReply(SOCKET sock);

#endif
