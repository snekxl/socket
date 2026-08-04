#include "network_tcp.h"
#include "ftp_parser.h"
#include <iostream>

using namespace std;

int main() {
    cout << "=== KHOI DONG HYBRID FTP CLIENT ===" << endl;

    // 1. Khởi tạo thư viện Winsock của Windows
    if (!initWinsock()) {
        cout << "[-] Khong the khoi tao Winsock. Dung chuong trinh." << endl;
        return 1;
    }

    // 2. Tạo socket cho Kênh điều khiển (TCP Control Channel)
    SOCKET control_sock = createTCPSocket();
    if (control_sock == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    // 3. Thiết lập thông số kết nối đến Server
    // (Mặc định test ở máy cục bộ localhost, port 2121. Bạn có thể đổi lại cho khớp với Server của nhóm)
    string serverIP = "127.0.0.1";
    int serverPort = 2121; 

    // 4. Bắt tay kết nối với Server
    if (connectToServer(control_sock, serverIP, serverPort)) {
        // Nếu kết nối thành công, đẩy quyền điều khiển sang vòng lặp Parser
        // Lúc này màn hình sẽ hiện "ftp> " để bạn gõ lệnh (như USER, CWD, LIST, QUIT)
        runCommandParser(control_sock);
    } else {
        cout << "[-] Vui long kiem tra xem Server da bat chua hoac xem lai IP/Port." << endl;
    }

    // 5. Dọn dẹp tài nguyên khi người dùng gõ lệnh QUIT để thoát
    closeTCPSocket(control_sock);
    cout << "[*] Client da dong hoan toan." << endl;
    
    return 0;
}
