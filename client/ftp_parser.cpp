#include "ftp_parser.h"
#include "network_udp.h"
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

#define DATA_PORT 2122 

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

    cout << receiveReply(control_sock);

    while (true) {
        cout << "ftp> ";
        getline(cin, userInput);

        if (userInput.empty()) continue;

        stringstream ss(userInput);
        string command, argument;
        ss >> command;
        getline(ss, argument);
        if (!argument.empty() && argument[0] == ' ') {
            argument.erase(0, 1);
        }

        if (!sendCommand(control_sock, userInput)) {
            cerr << "[-] Loi: Khong the gui lenh den Server." << endl;
            break;
        }

        if (command == "QUIT") {
            cout << "[*] Dang ngat ket noi TCP..." << endl;
            break;
        }

        string reply = receiveReply(control_sock);
        if (reply.empty()) {
            cerr << "[-] Loi: Mat ket noi voi Server." << endl;
            break;
        }
        cout << reply;

        if (reply.substr(0, 3) == "150") {

            SOCKET udpSocket = createUDPSocket();

            if (command == "RETR" && !argument.empty()) {
                cout << "[*] Chuan bi nhan file: " << argument << " qua UDP..." << endl;
                receiveFileUDP(udpSocket, DATA_PORT, serverIP, DATA_PORT, argument);
                cout << receiveReply(control_sock);
            }
            // Gom chung STOR, STOU, APPE vào luồng gửi dữ liệu
            else if ((command == "STOR" || command == "STOU" || command == "APPE") && !argument.empty()) {
                cout << "[*] Chuan bi gui file: " << argument << " qua UDP..." << endl;
                sendFileUDP(udpSocket, serverIP, DATA_PORT, argument);
                cout << receiveReply(control_sock);
            }
            else if (command == "LIST" || command == "NLST") {
                cout << "[*] Dang nhan danh sach thu muc qua UDP..." << endl;
                receiveFileUDP(udpSocket, DATA_PORT, serverIP, DATA_PORT, "LIST_RESULT.txt");
                cout << receiveReply(control_sock);
            }

            closeUDPSocket(udpSocket);
        }
    }
}
