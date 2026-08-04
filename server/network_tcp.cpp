#include "network_tcp.h"
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

// CONSTRUCTOR
TCPSocket::TCPSocket() {
    // Khởi tạo socket TCP (IPv4, TCP)
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        std::cerr << "[Lỗi] Không thể tạo socket TCP." << std::endl;
    }
}

// Constructor phụ: Dùng khi Server accept() ra một Client mới
TCPSocket::TCPSocket(int existing_fd) {
    sock_fd = existing_fd;
}

TCPSocket::~TCPSocket() {
    // closeSocket(); // Tùy chọn gọi hàm đóng trong destructor
}

// ---------------------------------------------------------
// HÀM CHO SERVER
// ---------------------------------------------------------

// 1. Gán Port cho Server
bool TCPSocket::bindPort(int port) {
    // Tránh lỗi kẹt Port khi tắt/mở Server liên tục
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Lắng nghe trên mọi IP của máy
    address.sin_port = htons(port);       // Hàm htons() ép kiểu Port về chuẩn mạng

    if (bind(sock_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "[Lỗi] Bind thất bại tại port " << port << std::endl;
        return false;
    }
    return true;
}

// 2. Mở cửa lắng nghe
bool TCPSocket::listenForClients(int backlog) {
    // backlog: Số lượng Client tối đa được phép xếp hàng chờ
    if (listen(sock_fd, backlog) < 0) {
        std::cerr << "[Lỗi] Listen thất bại." << std::endl;
        return false;
    }
    return true;
}

// 3. Chấp nhận kết nối từ Client
TCPSocket TCPSocket::acceptClient() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Lệnh accept() sẽ BLOCK (chặn) chương trình tại đây cho tới khi có khách
    int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_fd < 0) {
        std::cerr << "[Lỗi] Accept thất bại." << std::endl;
    } else {
        // In ra IP của Client vừa kết nối (tùy chọn)
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
        std::cout << "[Hệ thống] Có kết nối TCP mới từ IP: " << ip << std::endl;
    }
    
    // Trả về một đối tượng TCPSocket ĐỘC LẬP chứa thông tin của Client này
    return TCPSocket(client_fd); 
}

// ---------------------------------------------------------
// CÁC HÀM TIỆN ÍCH (Ví dụ minh họa)
// ---------------------------------------------------------
bool TCPSocket::isValid() const {
    return sock_fd != -1;
}

void TCPSocket::closeSocket() {
    if (sock_fd != -1) {
        close(sock_fd);
        sock_fd = -1;
    }
}
