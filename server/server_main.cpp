#include "ftp_handler.h"
#include <iostream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    cout << "=== HYBRID FTP SERVER ===" << endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Loi khoi tao Winsock!" << endl;
        return 1;
    }

    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2121);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5); 
    cout << "[TCP] Server dang lang nghe tren cong 2121..." << endl;

    while (true) {
        sockaddr_in client_info;
        int client_info_len = sizeof(client_info);
        SOCKET client_sock = accept(server_sock, (sockaddr*)&client_info, &client_info_len);

        if (client_sock == INVALID_SOCKET) continue;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_info.sin_addr), client_ip, INET_ADDRSTRLEN);

        mtx.lock();
        connected_clients[client_sock] = string(client_ip);
        cout << "\n[He thong] Phat hien ket noi moi tu (" << client_ip << ")!" << endl;
        printClientTable();
        mtx.unlock();

        thread(handleClientSession, client_sock, string(client_ip)).detach();
    }

    closesocket(server_sock);
    WSACleanup();
    return 0;
}
