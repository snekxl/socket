#ifndef FTP_PARSER_H
#define FTP_PARSER_H

#include <winsock2.h>
#include <string>

// Chạy vòng lặp nhập lệnh từ bàn phím
void runCommandParser(SOCKET control_sock);

// Hàm phụ trợ gửi lệnh (có tự động thêm \r\n theo chuẩn FTP)
bool sendCommand(SOCKET sock, const std::string& cmd);

// Hàm phụ trợ nhận phản hồi từ Server
std::string receiveReply(SOCKET sock);

#endif
