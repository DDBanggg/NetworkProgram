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
#include "../common/NetworkUtils.h"
using namespace std;

// --- HÀM CHẠY NGẦM NHẬN TIN NHẮN TỪ SERVER ---
void listenerThread(int socket) {
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;
        
        if (!NetworkUtils::recvPacket(socket, opcode, payload)) {
            cout << "\n[SYSTEM] Mat ket noi Server!" << endl;
            exit(0); 
        }

        PacketReader reader(payload);

        // --- XỬ LÝ CÁC USE CASE ---
        switch (opcode) {
            // Use Case 9: Nhận tin nhắn Real-time
            case NOTIFY_MSG_TEXT: {
                uint32_t topicId = reader.readInt();
                string sender = reader.readString();
                string msg = reader.readString();
                cout << "\n>>> [Topic " << topicId << "] " << sender << ": " << msg << endl;
                break;
            }

            // Use Case 4: Tạo Topic thành công
            case RES_CREATE_TOPIC: {
                uint8_t status = payload.empty() ? 1 : payload[0]; 
                if (status == TOPIC_CREATE_OK) {
                    // ID nằm sau byte status (offset 1)
                    if (payload.size() >= 5) {
                        uint32_t newId;
                        memcpy(&newId, &payload[1], 4);
                        newId = ntohl(newId);
                        cout << "\n[INFO] Tao Topic THANH CONG! ID: " << newId << endl;
                    }
                } else {
                    cout << "\n[ERROR] Tao Topic THAT BAI (Ten trung hoac loi he thong)." << endl;
                }
                break;
            }

            // Use Case 7: Xóa Topic
            case RES_DELETE_TOPIC: {
                uint8_t status = payload.empty() ? 1 : payload[0];
                if (status == TOPIC_DELETE_OK) cout << "\n[INFO] Xoa Topic THANH CONG!" << endl;
                else if (status == TOPIC_FAIL_DENIED) cout << "\n[ERROR] Ban khong phai chu Topic nay!" << endl;
                else cout << "\n[ERROR] Xoa Topic THAT BAI (Khong tim thay)." << endl;
                break;
            }

            // Use Case 9: Subscribe
            case RES_SUBSCRIBE: {
                uint8_t status = payload.empty() ? 1 : payload[0];
                if (status == SUB_OK) cout << "\n[INFO] Dang ky theo doi THANH CONG!" << endl;
                else cout << "\n[ERROR] Dang ky THAT BAI (Sai ID hoac da sub roi)." << endl;
                break;
            }

            // Use Case 10: Unsubscribe
            case RES_UNSUBSCRIBE: {
                uint8_t status = payload.empty() ? 1 : payload[0];
                if (status == UNSUB_OK) cout << "\n[INFO] Huy dang ky THANH CONG!" << endl;
                else cout << "\n[ERROR] Huy dang ky THAT BAI." << endl;
                break;
            }

            // Use Case 5 & 6: Xem danh sách Topic
            case RES_GET_ALL_TOPICS:
            case RES_GET_MY_TOPICS: {
                uint32_t count = reader.readInt();
                cout << "\n--- DANH SACH TOPIC (" << count << ") ---" << endl;
                for (uint32_t i = 0; i < count; ++i) {
                    string name = reader.readString();
                    string creator = reader.readString();
                    cout << "#" << (i + 1) << ". [" << name << "] - Creator: " << creator << endl;
                }
                cout << "-----------------------------------" << endl;
                break;
            }

            // Phản hồi gửi tin nhắn
            case RES_PUBLISH: {
            // Server trả về 1 byte trạng thái
            uint8_t status = payload.empty() ? 1 : payload[0];
            
            if (status == STATUS_SUCCESS) {
                // In nhẹ thông báo để user biết tin đã đi (tùy chọn)
                // cout << "\n[INFO] Da gui tin nhan." << endl;
            } else {
                cout << "\n[ERROR] Gui tin that bai (Server loi hoac Topic khong ton tai)." << endl;
            }
            break;
        }

            default:
                break;
        }

        cout << "Lua chon: " << flush;
    }
}
// -----------------------------------------------

void showMenu() {
    cout << "\n=== MENU CHINH ===" << endl;
    cout << "1. Dang Ky (Register)" << endl;
    cout << "2. Dang Nhap (Login)" << endl;
    cout << "3. Thoat (Exit)" << endl;
    cout << "Chon: ";
}

int main(int argc, char const *argv[]) {
    signal(SIGPIPE, SIG_IGN); 

    // CẤU HÌNH IP SERVER 
    string serverIP = "172.18.38.103"; 
    int serverPort = 8080;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort); 
    inet_pton(AF_INET, serverIP.c_str(), &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Khong the ket noi Server " << serverIP << endl;
        return -1;
    }

    ClientHandler handler(sock);
    bool isRunning = true;

    while (isRunning) {
        showMenu();
        int choice;
        if (!(cin >> choice)) { cin.clear(); cin.ignore(10000, '\n'); continue; }
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
                    // Kích hoạt Listener ngay sau khi Login thành công
                    std::thread t(listenerThread, sock);
                    t.detach(); 

                    bool loggedIn = true;
                    while (loggedIn) {
                        cout << "\n=== DASHBOARD (" << u << ") ===" << endl;
                        cout << "1. Xem DS Topic\t 2. Topic cua toi" << endl;
                        cout << "3. Tao Topic\t 4. Xoa Topic" << endl;
                        cout << "5. Subscribe\t 6. Unsubscribe" << endl;
                        cout << "7. Publish (Gui tin)\t\t 0. Dang xuat" << endl;
                        cout << "Lua chon: ";

                        int subChoice;
                        if (!(cin >> subChoice)) { cin.clear(); cin.ignore(10000, '\n'); continue; }
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
                                handler.requestSubscribe(tid);
                                break;
                            case 6:
                                cout << "ID Topic muon Unsub: "; cin >> tid; cin.ignore();
                                handler.requestUnsubscribe(tid);
                                break;
                            case 7: 
                                cout << "--- PUBLISH MESSAGE ---" << endl;
                                cout << "Nhap ID Topic muon gui tin: "; 
                                if (!(cin >> tid)) {
                                    cin.clear(); cin.ignore(10000, '\n');
                                    cout << ">> ID khong hop le!" << endl;
                                    break;
                                }
                                cin.ignore(); // Xóa bộ đệm sau khi nhập số

                                cout << "Nhap noi dung tin nhan: "; 
                                getline(cin, buf);

                                // Gọi Handler để đóng gói và gửi đi
                                if (handler.requestPublish(tid, buf)) {
                                    cout << ">> Da gui lenh Publish!" << endl;
                                } else {
                                    cout << ">> Loi gui tin (Mat ket noi?)" << endl;
                                }
                                break;
                            case 0:
                                loggedIn = false;
                                cout << ">> Dang xuat... (Vui long restart app de dang nhap lai)" << endl;
                                exit(0);
                                break;
                        }
                        this_thread::sleep_for(chrono::milliseconds(100));
                    }
                }
                break;
            case 3: isRunning = false; break;
        }
    }
    close(sock);
    return 0;
}