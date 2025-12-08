
#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "ClientHandler.h"

using namespace std;

void showMenu() {
    cout << "\n=== BANG TIN DIEN TU (SPRINT 1) ===" << endl;
    cout << "1. Dang Ky (Register)" << endl;
    cout << "2. Dang Nhap (Login)" << endl;
    cout << "3. Thoat (Exit)" << endl;
    cout << "Nhap lua chon: ";
}

int main(int argc, char const *argv[]) {
    //CẤU HÌNH SERVER
    string serverIP = "172.18.36.83"; 
    int serverPort = 8080;

    // 1. Kết nối tới Server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation error" << endl;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    
    // SỬ DỤNG BIẾN serverPort
    serv_addr.sin_port = htons(serverPort); 

    // Chuyển đổi IP: SỬ DỤNG BIẾN serverIP
    // Lưu ý: inet_pton cần tham số là char*, nên phải dùng .c_str()
    if (inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address/ Address not supported" << endl;
        return -1;
    }

    cout << "Connecting to server " << serverIP << ":" << serverPort << "..." << endl;
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection Failed. Is server running?" << endl;
        return -1;
    }
    cout << "Connected successfully!" << endl;

    // 2. Khởi tạo Handler
    ClientHandler handler(sock);

    // 3. Vòng lặp Menu (Main Thread)
    bool isRunning = true;
    while (isRunning) {
        showMenu();
        int choice;
        // Kiểm tra xem người dùng có nhập phải chữ cái gây lỗi lặp vô hạn không
        if (!(cin >> choice)) {
             cout << "Vui long nhap so!" << endl;
             cin.clear(); 
             cin.ignore(10000, '\n');
             continue;
        }

        cin.ignore(); // Xóa bộ đệm

        string u, p;
        switch (choice) {
            case 1: // Đăng ký
                cout << "--- DANG KY ---" << endl;
                cout << "Username: "; getline(cin, u);
                cout << "Password: "; getline(cin, p);
                handler.requestRegister(u, p);
                break;

            case 2: // Đăng nhập
                cout << "--- DANG NHAP ---" << endl;
                cout << "Username: "; getline(cin, u);
                cout << "Password: "; getline(cin, p);
                
                // --- BẮT ĐẦU MENU SPRINT 2 ---
                if (handler.requestLogin(u, p)) {
                    bool loggedIn = true;
                    while (loggedIn) {
                        cout << "\n=== DASHBOARD MENU (" << u << ") ===" << endl;
                        cout << "1. Xem tat ca Topic" << endl;
                        cout << "2. Tao Topic moi" << endl;
                        cout << "3. Topic cua toi" << endl;
                        cout << "4. Xoa Topic" << endl;
                        cout << "5. Dang xuat" << endl;
                        cout << "Lua chon: ";

                        int subChoice;
                        if (!(cin >> subChoice)) {
                            cin.clear(); cin.ignore(10000, '\n');
                            continue;
                        }
                        cin.ignore(); // Xóa bộ đệm

                        string buf;
                        switch (subChoice) {
                            case 1:
                                handler.requestGetList(false); // Get All
                                break;
                            case 2:
                                cout << "Nhap ten Topic moi: "; getline(cin, buf);
                                if (handler.requestCreateTopic(buf)) {
                                    cout << ">> Tao Topic thanh cong!" << endl;
                                }
                                break;
                            case 3:
                                handler.requestGetList(true); // Get My Topics
                                break;
                            case 4:
                                cout << "Nhap ten Topic can xoa: "; getline(cin, buf);
                                if (handler.requestDeleteTopic(buf)) {
                                    cout << ">> Xoa thanh cong!" << endl;
                                }
                                break;
                            case 5:
                                loggedIn = false;
                                cout << ">> Dang xuat thanh cong." << endl;
                                break;
                            default:
                                cout << "Lua chon khong hop le." << endl;
                        }
                    }
                }
                // --- KẾT THÚC MENU SPRINT 2 ---
                break;

            case 3: // Thoát
                isRunning = false;
                break;

            default:
                cout << "Lua chon khong hop le!" << endl;
        }
    }

    close(sock);
    return 0;
}