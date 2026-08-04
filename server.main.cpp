#include <iostream>
#include "network_tcp.h"
#include "network_udp.h" // File bạn cần tạo thêm
#include "ftp_parser.h"

int main() {
    // 1. Khởi tạo TCP Socket (Dùng hàm trong network_tcp)
    TCPSocket controlChannel;
    controlChannel.bind(2121);
    controlChannel.listen();
    
    std::cout << "Server dang lang nghe tren cong 2121..." << std::endl;

    while (true) {
        // Chờ Client kết nối
        TCPSocket clientConn = controlChannel.accept();
        clientConn.send("220 Welcome to Hybrid FTP C++ Server\r\n");

        while (true) {
            // Nhận chuỗi thô từ Client
            std::string rawData = clientConn.recv();
            if (rawData.empty()) break; // Client ngắt kết nối
            
            // 2. Dùng ftp_parser để phân tích lệnh
            FTPCommand cmd = FTPParser::parse(rawData);
            
            // 3. Xử lý logic
            if (cmd.type == "QUIT") {
                clientConn.send("221 Goodbye\r\n");
                break;
            } 
            else if (cmd.type == "RETR") {
                clientConn.send("150 Opening data channel...\r\n");
                
                // Mở kênh UDP (Dùng network_udp) truyền file
                UDPSocket dataChannel;
                dataChannel.sendFileReliable(cmd.argument); // Thuật toán Stop-and-Wait
                
                clientConn.send("226 Transfer complete\r\n");
            }
            // ... xử lý các lệnh khác theo doc ...
        }
    }
    return 0;
}
