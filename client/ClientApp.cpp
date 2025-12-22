
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
    string serverIP = "127.0.0.1"; 
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
                
                if (handler.requestLogin(u, p)) {
                    bool loggedIn = true;
                    while (loggedIn) {
                        cout << "\n=== DASHBOARD MENU (" << u << ") ===" << endl;
                        cout << "1. Xem ds Topic (Get All)" << endl;
                        cout << "2. Topic cua toi (My Topics)" << endl;
                        cout << "3. Tao Topic moi" << endl;
                        cout << "4. Xoa Topic" << endl;
                        // --- PHẦN MỚI ---
                        cout << "5. Subscribe (Theo doi Topic)" << endl;
                        cout << "6. Unsubscribe (Huy theo doi)" << endl;
                        cout << "7. Chat (Gui tin nhan)" << endl;
                        // ----------------
                        cout << "0. Dang xuat" << endl;
                        cout << "Lua chon: ";

                        int subChoice;
                        if (!(cin >> subChoice)) {
                            cin.clear(); cin.ignore(10000, '\n');
                            continue;
                        }
                        cin.ignore(); // Xóa bộ đệm sau khi nhập số

                        string buf;
                        uint32_t tId;

                        switch (subChoice) {
                            case 1:
                                handler.requestGetList(false); // Get All
                                break;
                            case 2:
                                handler.requestGetList(true); // Get My Topics
                                break;
                            case 3:
                                cout << "Nhap ten Topic moi: "; getline(cin, buf);
                                if (handler.requestCreateTopic(buf)) {
                                    cout << ">> Tao Topic thanh cong!" << endl;
                                }
                                break;
                            case 4:
                                cout << "Nhap ten Topic can xoa: "; getline(cin, buf);
                                if (handler.requestDeleteTopic(buf)) {
                                    cout << ">> Xoa thanh cong!" << endl;
                                }
                                break;
                            
                            // --- XỬ LÝ CHỨC NĂNG MỚI CHO MINH ANH ---
                            case 5: // Subscribe
                                cout << "Nhap ID Topic muon theo doi: "; 
                                cin >> tId; cin.ignore();
                                if (handler.requestSubscribe(tId)) cout << ">> Da Subscribe thanh cong topic " << tId << "!\n";
                                else cout << ">> Subscribe that bai (Sai ID hoac da sub roi)\n";
                                break;

                            case 6: // Unsubscribe
                                cout << "Nhap ID Topic muon huy theo doi: "; 
                                cin >> tId; cin.ignore();
                                if (handler.requestUnsubscribe(tId)) cout << ">> Da Unsubscribe topic " << tId << "!\n";
                                else cout << ">> Loi Unsubscribe.\n";
                                break;

                            case 7: // Chat
                                cout << "Nhap ID Topic muon chat: "; cin >> tId; cin.ignore();
                                cout << "Noi dung tin nhan: "; getline(cin, buf);
                                if (handler.requestPublish(tId, buf)) cout << ">> Da gui tin!\n";
                                else cout << ">> Gui loi (Chua Subscribe hoac Topic ko ton tai)\n";
                                break;
                            // ------------------------------------------

                            case 0:
                                loggedIn = false;
                                cout << ">> Dang xuat thanh cong." << endl;
                                break;
                            default:
                                cout << "Lua chon khong hop le." << endl;
                        }
                    }
                }
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