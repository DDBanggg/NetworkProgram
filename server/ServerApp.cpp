#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <map>          
#include <shared_mutex>  
#include <sys/stat.h>    
#include <cstdio>
#include <fstream>
#include "ServerHandler.h"

using namespace std;

#define PORT 8080

// =================================================================
// ĐỊNH NGHĨA BIẾN TOÀN CỤC (BẮT BUỘC ĐỂ LINK VỚI SERVERHANDLER)
// =================================================================
std::map<int, std::string> onlineUsers; 
std::shared_mutex clientsMutex; 
// =================================================================

// Hàm trung gian để chạy Handler trong luồng mới
void connectionHandler(int clientSock) {
    // Tạo đối tượng Handler trên stack của luồng này
    ServerHandler handler(clientSock);
    handler.run(); 
    // Khi run() kết thúc (client disconnect), handler tự hủy -> đóng socket
}

// Hàm dọn dẹp dữ liệu 
void cleanSessionData() {
    // 1. Làm rỗng các file text (Topics, History, Log)
    std::ofstream f1("data/topics.txt", std::ios::trunc); f1.close();
    std::ofstream f2("topic_history.txt", std::ios::trunc); f2.close();
    std::ofstream f3("server_log.txt", std::ios::trunc); f3.close();

    // 2. Xóa các file trong thư mục server_storage
    int ret = system("rm -f server_storage/*");
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    // --- TẠO THƯ MỤC DỮ LIỆU NẾU CHƯA CÓ ---
    #ifdef _WIN32
        _mkdir("data");
        _mkdir("server_storage");
    #else
        mkdir("data", 0777);
        mkdir("server_storage", 0777);
    #endif
    // ------------------------------------------------------------------

    cleanSessionData(); // Dọn dẹp dữ liệu cũ

    ServerHandler::loadData();

    int serverSocket, newSocket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. Tạo Socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        return -1;
    }

    // Tùy chọn để tái sử dụng cổng ngay sau khi tắt server
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Gán địa chỉ IP/Port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Lắng nghe mọi IP
    address.sin_port = htons(PORT);

    // 3. Bind
    if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        return -1;
    }

    // 4. Listen
    if (listen(serverSocket, 10) < 0) {
        perror("Listen failed");
        return -1;
    }

    cout << "Server dang lang nghe tai port " << PORT << "...\n";

    // 5. Vòng lặp chấp nhận kết nối
    while (true) {
        if ((newSocket = accept(serverSocket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Cài đặt Timeout 5 phút
        struct timeval tv;
        tv.tv_sec = 300; 
        tv.tv_usec = 0;
        if (setsockopt(newSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
            perror("Setsockopt timeout failed");
        }

        cout << "Ket noi moi! Socket ID: " << newSocket << endl;

        // 6. Tạo luồng mới
        thread t(connectionHandler, newSocket);
        t.detach();
    }

    return 0;
}