#include "ftp_parser.h"
#include <iostream>
#include <string>

using namespace std;

bool sendCommand(SOCKET sock, const string& cmd) {
    // Theo chuẩn FTP (RFC 959), mọi lệnh gửi đi đều phải kết thúc bằng CRLF (\r\n)
    string formatted_cmd = cmd + "\r\n"; 
    int bytesSent = send(sock, formatted_cmd.c_str(), formatted_cmd.length(), 0);
    
    if (bytesSent == SOCKET_ERROR) {
        return false;
    }
    return true;
}

string receiveReply(SOCKET sock) {
    char buffer[4096];
    // Lắng nghe TCP Control Channel xem Server trả về mã gì
    int bytesRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRecv > 0) {
        buffer[bytesRecv] = '\0'; // Kết thúc chuỗi an toàn
        return string(buffer);
    }
    return ""; // Trả về rỗng nếu lỗi hoặc mất kết nối
}

void runCommandParser(SOCKET control_sock) {
    string userInput;
    cout << "=== HYBRID FTP CLIENT ===" << endl;
    cout << "[+] Da ket noi Control Channel. Nhap lenh FTP (vd: CWD, LIST, QUIT)..." << endl;

    // Vòng lặp chính của phần mềm Client
    while (true) {
        cout << "ftp> ";
        getline(cin, userInput);

        // Bỏ qua nếu lỡ nhấn Enter mà chưa gõ gì
        if (userInput.empty()) continue;

        // 1. Gửi lệnh qua kênh TCP
        if (!sendCommand(control_sock, userInput)) {
            cerr << "[-] Loi: Khong the gui lenh den Server." << endl;
            break;
        }

        // Thoát ngay nếu người dùng gõ QUIT
        if (userInput == "QUIT") {
            cout << "[*] Dang ngat ket noi TCP..." << endl;
            break;
        }

        // 2. Chờ Server phản hồi (vd: 250 Requested file action OK)
        string reply = receiveReply(control_sock);
        if (reply.empty()) {
            cerr << "[-] Loi: Mat ket noi voi Server." << endl;
            break;
        }

        // In nguyên văn câu trả lời của Server ra màn hình
        cout << reply;
    }
}
