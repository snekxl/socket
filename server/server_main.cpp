#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

// Bắt buộc trên Visual Studio/Windows để chạy thư viện mạng
#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    cout << "=== HYBRID FTP SERVER ===" << endl;

    // 1. Khởi tạo Winsock (Giống hệt Client)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Loi: Khong the khoi tao Winsock!" << endl;
        return 1;
    }

    // 2. Tạo socket Server
    SOCKET server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock == INVALID_SOCKET) {
        cerr << "Loi tao socket!" << endl;
        WSACleanup();
        return 1;
    }

    // 3. Cấu hình IP và Port (2121)
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(2121);
    server_addr.sin_addr.s_addr = INADDR_ANY; // Lắng nghe mọi IP

    // 4. Bind và Listen
    bind(server_sock, (sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 1);
    cout << "[TCP] Server dang lang nghe tren cong 2121..." << endl;

    // 5. Chấp nhận kết nối từ Client
    SOCKET client_sock = accept(server_sock, NULL, NULL);
    if (client_sock != INVALID_SOCKET) {
        cout << "[Hệ thống] Client da ket noi thanh cong!" << endl;

        // BẮT BUỘC: Gửi lời chào 220 ngay khi kết nối để Client biết Server đã sẵn sàng
        string welcome_msg = "220 Welcome to Hybrid FTP Server\r\n";
        send(client_sock, welcome_msg.c_str(), welcome_msg.length(), 0);

        // 6. Vòng lặp nhận lệnh và phản hồi
        char buffer[1024];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                cout << "[Hệ thống] Client da ngat ket noi." << endl;
                break;
            }

            string request(buffer);
            cout << "[CLIENT] " << request; // In lệnh nhận được (đã có sẵn \r\n từ client)

            string response = "";

            // --- BẮT ĐẦU XỬ LÝ LỆNH TỪ CLIENT ---
            if (request.find("QUIT") == 0) {
                response = "221 Goodbye. Hen gap lai!\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
                break; // Thoát vòng lặp
            }
            else if (request.find("USER") == 0) {
                response = "331 Username OK, need password.\r\n";
            }
            else if (request.find("PASS") == 0) {
                response = "230 User logged in, proceed.\r\n";
            }
            else if (request.find("RETR") == 0) {
                response = "150 File status okay; about to open data connection (UDP).\r\n";
                // TODO: Chèn logic gọi UDP truyền file ở đây
            }
            else {
                response = "502 Command not implemented.\r\n";
            }

            // Gửi phản hồi lại cho Client (để hàm receiveTCP() của client được giải phóng)
            send(client_sock, response.c_str(), response.length(), 0);
        }
        closesocket(client_sock);
    }

    // 7. Dọn dẹp
    closesocket(server_sock);
    WSACleanup();
    cout << "Server da dong." << endl;
    return 0;
}
