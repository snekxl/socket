#include "network_tcp.h"
#include "ftp_parser.h"
#include <iostream>

using namespace std;

int main() {
    cout << "=== KHOI DONG HYBRID FTP CLIENT ===" << endl;

    if (!initWinsock()) {
        cout << "[-] Khong the khoi tao Winsock. Dung chuong trinh." << endl;
        return 1;
    }

    SOCKET control_sock = createTCPSocket();
    if (control_sock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    string serverIP = "127.0.0.1";
    int serverPort = 2121; 

    if (connectToServer(control_sock, serverIP, serverPort)) {
        // Truyền thêm serverIP vào để UDP biết địa chỉ mà đẩy file tới
        runCommandParser(control_sock, serverIP);
    } else {
        cout << "[-] Vui long kiem tra xem Server da bat chua hoac xem lai IP/Port." << endl;
    }

    closeTCPSocket(control_sock);
    cout << "[*] Client da dong hoan toan." << endl;
    
    return 0;
}
