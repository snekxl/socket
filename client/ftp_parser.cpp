#include "ftp_parser.h"
#include "network_tcp.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

void runCommandParser(SOCKET control_sock) {
    string input_line;
    cout << "=== CLIENT DA KHOI DONG ===" << endl;
    cout << "San sang nhap lenh FTP..." << endl;

    while (true) {
        cout << "ftp> ";
        getline(cin, input_line);

        if (input_line.empty()) continue;

        stringstream ss(input_line);
        string command, argument;

        ss >> command;
        getline(ss >> ws, argument);

        transform(command.begin(), command.end(), command.begin(), ::toupper);

        string ftp_command = "";
        bool should_exit = false;

        // Nhóm 1: Bắt buộc có tham số
        if (command == "USER" || command == "PASS" || command == "CWD" || command == "MKD" ||
            command == "RMD" || command == "SIZE" || command == "MDTM" || command == "TYPE" ||
            command == "MODE" || command == "PORT" || command == "RETR" || command == "STOR" ||
            command == "APPE" || command == "DELE" || command == "RNFR" || command == "RNTO" ||
            command == "HASH") {

            if (argument.empty()) {
                cout << "501 Syntax error in parameters (Thieu tham so cho lenh " << command << ")." << endl;
                continue;
            }
            ftp_command = command + " " + argument + "\r\n";
        }
        // Nhóm 2: Không cần tham số
        else if (command == "QUIT" || command == "NOOP" || command == "PWD" ||
            command == "CDUP" || command == "PASV" || command == "STOU" || command == "ABOR") {
            if (command == "QUIT") {
                cout << "Chuan bi dong ket noi TCP..." << endl;
                should_exit = true;
            }
            ftp_command = command + "\r\n";
        }
        // Nhóm 3: Tham số tùy chọn
        else if (command == "LIST" || command == "NLST" || command == "STAT" || command == "HELP") {
            if (argument.empty()) ftp_command = command + "\r\n";
            else ftp_command = command + " " + argument + "\r\n";
        }
        else {
            cout << "502 Command not implemented (Lenh khong ton tai hoac sai cu phap)." << endl;
            continue;
        }

        // Tiến hành gửi qua TCP
        if (!ftp_command.empty()) {
            if (control_sock != INVALID_SOCKET) {
                int bytes_sent = send(control_sock, ftp_command.c_str(), ftp_command.length(), 0);
                if (bytes_sent != SOCKET_ERROR) {
                    cout << "[TCP] Da gui: " << ftp_command;
                    string response = receiveTCP(control_sock);
                    if (!response.empty()) {
                        cout << "[SERVER] " << response;
                    }
                }
                else {
                    cerr << "[TCP] Loi gui du lieu: " << WSAGetLastError() << endl;
                }
            }
            else {
                cout << "[Offline] Se gui: " << ftp_command;
            }
        }

        if (should_exit) break;
    }
}