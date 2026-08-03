#pragma once
#include <string>
#include <winsock2.h> // Bổ sung thư viện này để nhận diện kiểu SOCKET

// Truyền SOCKET vào để bộ phân tích biết đường gửi lệnh
void runCommandParser(SOCKET control_sock);