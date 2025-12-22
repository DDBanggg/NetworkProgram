#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <signal.h> 
#include "ClientHandler.h"
#include "../common/DataUtils.h" 
#include "../common/protocol.h"

using namespace std;

// --- HÀM CHẠY NGẦM NHẬN TIN NHẮN TỪ SERVER ---
void listenerThread(int socket) {
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;
        
        // Hàm này sẽ block cho đến khi Server gửi gì đó về
        if (!NetworkUtils::recvPacket(socket, opcode, payload)) {
            cout << "\n[SYSTEM] Mat ket noi Server (Listener stopped)!" << endl;
            exit(0); // Thoát chương trình nếu mất mạng
        }

        // Nếu là tin nhắn Chat (OpCode 28 - NOTIFY_MSG_TEXT)
        if (opcode == NOTIFY_MSG_TEXT) {
            PacketReader reader(payload);
            uint32_t topicId = reader.readInt();
            string sender = reader.readString();
            string msg = reader.readString();
            
            // In đè lên màn hình
            cout << "\n\n>>> [Topic " << topicId << "] " << sender << ": " << msg << endl;
            cout << "Lua chon: " << flush; // In lại dấu nhắc nhập liệu
        }
    }
}

void showMenu() {
    cout << "\n=== MENU CHINH ===" << endl;
    cout << "1. Dang Ky (Register)" << endl;
    cout << "2. Dang Nhap (Login)" << endl;
    cout << "3. Thoat (Exit)" << endl;
    cout << "Chon: ";
}

int main(int argc, char const *argv[]) {
    // Bỏ qua tín hiệu SIGPIPE để tránh crash khi mất kết nối đột ngột
    signal(SIGPIPE, SIG_IGN); 

    // CẤU HÌNH SERVER
    string serverIP = "127.0.0.1"; 
    int serverPort = 8080;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        cerr << "Socket creation error" << endl;
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort); 

    if (inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr) <= 0) {
        cerr << "Invalid address" << endl;
        return -1;
    }

    cout << "Dang ket noi den server " << serverIP << ":" << serverPort << "..." << endl;
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Ket noi that bai. Server da bat chua?" << endl;
        return -1;
    }
    cout << "Ket noi thanh cong!" << endl;

    ClientHandler handler(sock);
    bool isRunning = true;

    while (isRunning) {
        showMenu();
        int choice;
        if (!(cin >> choice)) {
            cin.clear(); cin.ignore(10000, '\n');
            continue;
        }
        cin.ignore(); 

        string u, p;
        switch (choice) {
            case 1: 
                cout << "User: "; getline(cin, u);
                cout << "Pass: "; getline(cin, p);
                handler.requestRegister(u, p);
                break;

            case 2:
                cout << "User: "; getline(cin, u);
                cout << "Pass: "; getline(cin, p);
                
                if (handler.requestLogin(u, p)) {
                    //  Login thành công -> Bật luồng nghe tin nhắn
                    std::thread t(listenerThread, sock);
                    t.detach(); // Tách luồng để chạy ngầm vĩnh viễn

                    bool loggedIn = true;
                    while (loggedIn) {
                        cout << "\n=== DASHBOARD (" << u << ") ===" << endl;
                        cout << "1. Xem DS Topic\t 2. Topic cua toi" << endl;
                        cout << "3. Tao Topic\t 4. Xoa Topic" << endl;
                        cout << "5. Subscribe\t 6. Unsubscribe" << endl;
                        cout << "7. Chat\t\t 0. Dang xuat" << endl;
                        cout << "Lua chon: ";

                        int subChoice;
                        if (!(cin >> subChoice)) {
                            cin.clear(); cin.ignore(10000, '\n'); continue;
                        }
                        cin.ignore();

                        string buf; uint32_t tid;
                        switch (subChoice) {
                            case 1: handler.requestGetList(false); break;
                            case 2: handler.requestGetList(true); break;
                            case 3:
                                cout << "Ten Topic moi: "; getline(cin, buf);
                                handler.requestCreateTopic(buf);
                                break;
                            case 4:
                                cout << "Ten Topic xoa: "; getline(cin, buf);
                                handler.requestDeleteTopic(buf);
                                break;
                            case 5:
                                cout << "ID Topic muon Sub: "; cin >> tid; cin.ignore();
                                if(handler.requestSubscribe(tid)) cout << ">> Sub thanh cong!\n";
                                else cout << ">> Sub that bai.\n";
                                break;
                            case 6:
                                cout << "ID Topic muon Unsub: "; cin >> tid; cin.ignore();
                                if(handler.requestUnsubscribe(tid)) cout << ">> Unsub thanh cong!\n";
                                else cout << ">> Unsub that bai.\n";
                                break;
                            case 7:
                                cout << "ID Topic muon chat: "; cin >> tid; cin.ignore();
                                cout << "Noi dung: "; getline(cin, buf);
                                handler.requestPublish(tid, buf);
                                break;
                            case 0:
                                loggedIn = false;
                                // Luồng listener vẫn đang chạy và có thể in ra thông báo lỗi khi ngắt socket.
                                cout << ">> Dang xuat...\n";
                                exit(0);
                                break;
                            default: cout << "Lua chon sai.\n";
                        }
                    }
                }
                break;

            case 3: isRunning = false; break;
            default: cout << "Lua chon khong hop le!" << endl;
        }
    }

    close(sock);
    return 0;
}