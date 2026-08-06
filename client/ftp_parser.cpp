#include "ftp_parser.h"
#include "network_udp.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

#define DATA_PORT 2122 // Cổng mặc định cho Data Channel (UDP)

bool sendCommand(SOCKET sock, const string& cmd) {
    string formatted_cmd = cmd + "\r\n"; 
    int bytesSent = send(sock, formatted_cmd.c_str(), formatted_cmd.length(), 0);
    return bytesSent != SOCKET_ERROR;
}

string receiveReply(SOCKET sock) {
    char buffer[4096];
    int bytesRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRecv > 0) {
        buffer[bytesRecv] = '\0';
        return string(buffer);
    }
    return "";
}

void runCommandParser(SOCKET control_sock, const string& serverIP) {
    string userInput;
    cout << "=== HYBRID FTP CLIENT ===" << endl;
    cout << "[+] Da ket noi Control Channel. Nhap lenh FTP (vd: USER, CWD, RETR, STOR, QUIT)..." << endl;

    // Tạo sẵn 1 socket UDP để dùng chung cho các phiên truyền file sau này
    SOCKET udpSocket = createUDPSocket();

    while (true) {
        cout << "ftp> ";
        getline(cin, userInput);

        if (userInput.empty()) continue;

        // Phân tích câu lệnh gõ vào (tách lấy phần chữ đầu tiên và tham số)
        stringstream ss(userInput);
        string command, argument;
        ss >> command;
        getline(ss, argument);
        // Xóa khoảng trắng thừa ở đầu argument (nếu có)
        if (!argument.empty() && argument[0] == ' ') {
            argument.erase(0, 1);
        }

        // 1. Gửi lệnh qua kênh TCP
        if (!sendCommand(control_sock, userInput)) {
            cerr << "[-] Loi: Khong the gui lenh den Server." << endl;
            break;
        }

        if (command == "QUIT") {
            cout << "[*] Dang ngat ket noi TCP..." << endl;
            break;
        }

        // 2. Chờ Server phản hồi lệnh vừa gửi
        string reply = receiveReply(control_sock);
        if (reply.empty()) {
            cerr << "[-] Loi: Mat ket noi voi Server." << endl;
            break;
        }

        // In nguyên văn câu trả lời của Server
        cout << reply;

        // 3. Xử lý rẽ nhánh nếu là lệnh truyền file (RETR hoặc STOR)
        // Dựa vào mã phản hồi 150 (Mở kênh dữ liệu) từ Server
        if (reply.substr(0, 3) == "150") {
            if (command == "RETR" && !argument.empty()) {
                cout << "[*] Chuan bi nhan file: " << argument << " qua UDP..." << endl;
                
                // Kích hoạt luồng UDP nhận file
                receiveFileUDP(udpSocket, DATA_PORT, argument);
                
                // Hứng thêm phản hồi 226 Transfer complete từ TCP sau khi truyền xong
                cout << receiveReply(control_sock);
            } 
            else if (command == "STOR" && !argument.empty()) {
                cout << "[*] Chuan bi gui file: " << argument << " qua UDP..." << endl;
                
                // Kích hoạt luồng UDP đẩy file
                sendFileUDP(udpSocket, serverIP, DATA_PORT, argument);
                
                // Hứng thêm phản hồi 226 Transfer complete từ TCP sau khi truyền xong
                cout << receiveReply(control_sock);
            }
        }
    }
    
    // Đóng kênh UDP khi thoát vòng lặp
    closeUDPSocket(udpSocket);
}
