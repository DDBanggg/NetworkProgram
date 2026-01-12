#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>      // Thêm thư viện file
#include <signal.h> 
#include "ClientHandler.h"
#include "../common/DataUtils.h" 
#include "../common/protocol.h"
#include "../common/NetworkUtils.h"
#include <chrono>
#include <ctime>
#include <sstream>

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

        switch (opcode) {
            // --- USE CASE: NHẬN TIN NHẮN TEXT ---
            case NOTIFY_MSG_TEXT: {
                uint32_t topicId = reader.readInt();
                string sender = reader.readString();
                string msg = reader.readString();
                cout << "\n>>> [Topic " << topicId << "] " << sender << ": " << msg << endl;
                break;
            }

            // --- USE CASE: NHẬN FILE (ẢNH/AUDIO) ---
            case NOTIFY_MSG_BIN: {
                uint32_t topicId = reader.readInt();
                string sender = reader.readString();
                string ext = reader.readString();
                uint32_t dataLen = reader.readInt();
                
                // Đọc phần dữ liệu binary còn lại
                // Lưu ý: PacketReader cần hỗ trợ hàm lấy con trỏ data hiện tại
                // Ở đây ta giả sử reader đã đọc đến phần data
                
                // Tạo tên file unique
                auto now = std::chrono::system_clock::now();
                auto now_c = std::chrono::system_clock::to_time_t(now);
                stringstream ss;
                ss << "received_" << now_c << ext;
                string filename = ss.str();

                // Ghi ra file
                ofstream outFile(filename, ios::binary);
                // Dữ liệu binary nằm ở cuối payload. 
                // Cách đơn giản nhất: tính offset
                size_t headerSize = 4 + 4 + sender.length() + 4 + ext.length() + 4; 
                if (payload.size() > headerSize) {
                    outFile.write((char*)payload.data() + headerSize, payload.size() - headerSize);
                }
                outFile.close();

                cout << "\n>>> [Topic " << topicId << "] " << sender << " da gui 1 file: " << filename << endl;
                break;
            }

            // --- CÁC PHẢN HỒI KHÁC ---
            case RES_CREATE_TOPIC: {
                uint8_t status = payload.empty() ? 1 : payload[0]; 
                if (status == TOPIC_CREATE_OK) {
                    // ID nằm sau byte status
                    if (payload.size() >= 5) {
                        uint32_t newId;
                        memcpy(&newId, &payload[1], 4);
                        newId = ntohl(newId); // Chuyển Endian
                        cout << "\n[INFO] Tao Topic THANH CONG! ID: " << newId << endl;
                    }
                } else {
                    cout << "\n[ERROR] Tao Topic THAT BAI." << endl;
                }
                break;
            }
            
            // Xử lý thông tin Topic (TC08, TC10)
            case RES_TOPIC_INFO: {
                string creator = reader.readString();
                uint32_t subCount = reader.readInt();
                cout << "\n[INFO] Creator: " << creator << " | Subscribers: " << subCount << endl;
                break;
            }

            // Xử lý danh sách Sub (TC09)
            case RES_TOPIC_SUBS: {
                uint32_t count = reader.readInt();
                cout << "\n--- DANH SACH NGUOI DANG KY (" << count << ") ---" << endl;
                for(uint32_t i=0; i<count; i++) {
                    cout << "- " << reader.readString() << endl;
                }
                break;
            }

            case RES_DELETE_TOPIC: {
                uint8_t status = payload.empty() ? 1 : payload[0];
                if (status == TOPIC_DELETE_OK) cout << "\n[INFO] Xoa Topic THANH CONG!" << endl;
                else cout << "\n[ERROR] Xoa Topic THAT BAI (Khong phai chu so huu)." << endl;
                break;
            }

            case RES_SUBSCRIBE: {
                uint8_t status = payload.empty() ? 1 : payload[0];
                if (status == SUB_OK) cout << "\n[INFO] Dang ky theo doi THANH CONG!" << endl;
                else cout << "\n[ERROR] Dang ky THAT BAI." << endl;
                break;
            }

            case RES_UNSUBSCRIBE: {
                 cout << "\n[INFO] Huy dang ky THANH CONG!" << endl;
                 break;
            }

            case RES_GET_ALL_TOPICS:

            case RES_GET_MY_TOPICS: {
                uint32_t count = reader.readInt();
                cout << "\n--- DANH SACH TOPIC (" << count << ") ---" << endl;
                for (uint32_t i = 0; i < count; ++i) {
                    // Server giờ gửi: ID, Name, Desc, Creator
                    uint32_t id = reader.readInt(); 
                    string name = reader.readString();
                    string desc = reader.readString();
                    string creator = reader.readString();
                    cout << "#" << id << ". [" << name << "] - " << desc << " (by " << creator << ")" << endl;
                }
                cout << "-----------------------------------" << endl;
                break;
            }

            case RES_HISTORY_START: {
                uint32_t count = reader.readInt();
                cout << "\n--- DANG TAI " << count << " TIN NHAN CU ---" << endl;
                break;
            }

            case RES_HISTORY_ITEM: {
                uint8_t type = 0;
                memcpy(&type, payload.data(), 1); // Đọc 1 byte type đầu tiên
                PacketReader historyReader(payload.data() + 1, payload.size() - 1);
                string sender = historyReader.readString();
                string msg = historyReader.readString();
                cout << "  (Lich su) " << sender << ": " << msg << endl;
                break;
            }

            case RES_HISTORY_END: {
                cout << "--- KET THUC LICH SU ---" << endl;
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

    // CẤU HÌNH IP LOCALHOST
    string serverIP = "127.0.0.1"; 
    int serverPort = 8080;

    // Cho phép nhập IP nếu muốn (tùy chọn)
    cout << "Nhap IP Server (Enter de dung 127.0.0.1): ";
    string inputIP;
    getline(cin, inputIP);
    if (!inputIP.empty()) serverIP = inputIP;

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
                    // Kích hoạt Listener
                    std::thread t(listenerThread, sock);
                    t.detach(); 

                    bool loggedIn = true;
                    while (loggedIn) {
                        cout << "\n=== DASHBOARD (" << u << ") ===" << endl;
                        cout << "1. Xem DS Topic\t 2. Topic cua toi" << endl;
                        cout << "3. Tao Topic\t 4. Xoa Topic" << endl;
                        cout << "5. Subscribe\t 6. Unsubscribe" << endl;
                        cout << "7. Gui Tin (Text)\t 8. Gui File (Anh/Audio)" << endl;
                        cout << "9. Lich Su\t 10. Thong tin Topic" << endl;
                        cout << "11. Xem Subs\t 0. Dang xuat" << endl;
                        cout << "Lua chon: ";

                        int subChoice;
                        if (!(cin >> subChoice)) { cin.clear(); cin.ignore(10000, '\n'); continue; }
                        cin.ignore();

                        string buf, desc; 
                        uint32_t tid;
                        
                        switch (subChoice) {
                            case 1: handler.requestGetList(false); break;
                            case 2: handler.requestGetList(true); break;
                            case 3:
                                cout << "Ten Topic: "; getline(cin, buf);
                                cout << "Mo ta: "; getline(cin, desc);
                                // LƯU Ý: Bạn cần cập nhật ClientHandler::requestCreateTopic để nhận 2 tham số
                                handler.requestCreateTopic(buf, desc); 
                                break;
                            case 4: {
    string tName;
    cout << "Ten Topic xoa: "; 
    getline(cin, tName); // Nhập tên thay vì ID
    handler.requestDeleteTopic(tName); // Cần sửa hàm này nhận string
    break;
}
                            case 5:
                                cout << "ID Topic Sub: "; cin >> tid; cin.ignore();
                                handler.requestSubscribe(tid);
                                break;
                            case 6:
                                cout << "ID Topic Unsub: "; cin >> tid; cin.ignore();
                                handler.requestUnsubscribe(tid);
                                break;
                            case 7: 
                                cout << "ID Topic: "; cin >> tid; cin.ignore();
                                cout << "Noi dung: "; getline(cin, buf);
                                handler.requestPublish(tid, buf);
                                break;
                            case 8: // Gửi File
                                cout << "ID Topic: "; cin >> tid; cin.ignore();
                                cout << "Duong dan File (VD: test.jpg): "; getline(cin, buf);
                                handler.requestPublishBinary(tid, buf);
                                break;
                            case 9: // Lịch sử
                                cout << "ID Topic: "; cin >> tid; cin.ignore();
                                handler.requestHistory(tid);
                                break;
                            case 10: // Info
                                cout << "ID Topic: "; cin >> tid; cin.ignore();
                                handler.requestTopicInfo(tid);
                                break;
                            case 11: // List Subs
                                cout << "ID Topic: "; cin >> tid; cin.ignore();
                                handler.requestTopicSubs(tid);
                                break;
                            case 0:
                                loggedIn = false;
                                exit(0);
                                break;
                        }
                        this_thread::sleep_for(chrono::milliseconds(200));
                    }
                }
                break;
            case 3: isRunning = false; break;
        }
    }
    close(sock);
    return 0;
}