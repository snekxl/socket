#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "ftp_parser.h"
#include "network_tcp.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    cout << "=== HYBRID FTP CLIENT ===" << endl;

    // 1. Khởi tạo mạng và kết nối TCP
    SOCKET control_sock = connectTCP("127.0.0.1", 2121);

    // 2. Chạy vòng lặp phân tích lệnh, truyền ống nước TCP vào
    runCommandParser(control_sock);

    // 3. Dọn dẹp trước khi thoát
    cleanupTCP(control_sock);

    return 0;
}