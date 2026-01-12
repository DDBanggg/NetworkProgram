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
#include <fstream>
#include <vector>
#include <chrono> // Dùng cho timestamp
using namespace std;

// --- HÀM CHẠY NGẦM NHẬN TIN NHẮN TỪ SERVER ---
void listenerThread(int socket) {
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;
        
        // Nhận packet (Header + Payload)
        if (!NetworkUtils::recvPacket(socket, opcode, payload)) {
            cout << "\n[SYSTEM] Mat ket noi Server!" << endl;
            exit(0); 
        }

        PacketReader reader(payload);

        switch (opcode) {
            // ... (Các case cũ giữ nguyên) ...
            case NOTIFY_MSG_TEXT: {
                uint32_t topicId = reader.readInt();
                string sender = reader.readString();
                string msg = reader.readString();
                cout << "\n>>> [Topic " << topicId << "] " << sender << ": " << msg << endl;
                break;
            }

            // --- CASE MỚI: NHẬN FILE ---
            case NOTIFY_MSG_BIN: { // OpCode 29
                // Protocol: [TopicID] [Sender] [Ext] [Data]
                uint32_t topicId = reader.readInt();
                string sender = reader.readString();
                string ext = reader.readString();
                string data = reader.readString(); // Đọc binary data vào string

                // Tạo tên file output: received_<timestamp>.<ext>
                auto now = std::chrono::system_clock::now().time_since_epoch();
                long long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                string outFileName = "received_" + to_string(timestamp) + "." + ext;

                // Ghi ra đĩa
                ofstream outFile(outFileName, ios::binary);
                if (outFile.is_open()) {
                    outFile.write(data.data(), data.size());
                    outFile.close();
                    cout << "\n>>> [Topic " << topicId << "] " << sender 
                         << " sent a file: " << outFileName 
                         << " (" << data.size() << " bytes)" << endl;
                } else {
                    cout << "\n>>> [ERROR] Khong the luu file: " << outFileName << endl;
                }
                break;
            }
            
            // ... (Các case khác giữ nguyên)
        }
        
        // FIX UI GLITCH: In lại dấu nhắc sau khi hiện thông báo
        cout << "Lua chon: " << flush;
    }
}

void showMenu() {
    cout << "\n=== MENU CHINH ===" << endl;
    cout << "1. Dang Ky (Register)" << endl;
    cout << "2. Dang Nhap (Login)" << endl;
    cout << "3. Thoat (Exit)" << endl;
    cout << "Chon: ";
}

// --- CHỨC NĂNG GỬI FILE (PRODUCER) ---
void uploadFile(int socket, uint32_t topicId, const string& filePath) {
    // 1. Mở file chế độ Binary
    ifstream file(filePath, ios::binary | ios::ate); // ios::ate để con trỏ ở cuối file (lấy size)
    if (!file.is_open()) {
        cout << ">> [ERROR] Khong tim thay file: " << filePath << endl;
        return;
    }

    // 2. Lấy kích thước file
    streamsize size = file.tellg();
    file.seekg(0, ios::beg); // Quay về đầu để đọc

    if (size <= 0) {
        cout << ">> [ERROR] File rong!" << endl;
        return;
    }
    
    // Giới hạn file 10MB (tuỳ chọn)
    if (size > 10 * 1024 * 1024) { 
        cout << ">> [ERROR] File qua lon (>10MB)!" << endl;
        return;
    }

    // 3. Đọc dữ liệu vào buffer
    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        cout << ">> [ERROR] Loi doc file!" << endl;
        return;
    }
    file.close();

    // 4. Lấy đuôi file (Extension)
    string extension = "bin";
    size_t dotPos = filePath.find_last_of(".");
    if (dotPos != string::npos) {
        extension = filePath.substr(dotPos + 1);
    }

    // 5. Đóng gói Packet (REQ_PUBLISH_BIN - 26)
    // Cấu trúc: [TopicID 4B] + [Ext_Len 4B + Ext] + [Data_Len 4B + Data]
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(extension);
    
    // Chuyển vector<char> sang string để dùng addString (DataUtils hỗ trợ safe-binary string)
    // addString sẽ tự động thêm 4 byte độ dài trước dữ liệu
    string dataStr(buffer.begin(), buffer.end()); 
    builder.addString(dataStr);

    // 6. Gửi đi
    if (NetworkUtils::sendPacket(socket, REQ_PUBLISH_BIN, builder.getData(), builder.getSize())) {
        cout << ">> [INFO] Da gui file " << filePath << " (" << size << " bytes)." << endl;
    } else {
        cout << ">> [ERROR] Loi gui packet!" << endl;
    }
}

void requestHistory(int socket, uint32_t topicId) {
    // 1. Đóng gói gói tin REQ_HISTORY (OpCode 30)
    // Payload: [TopicID 4B]
    PacketBuilder builder;
    builder.addInt(topicId);

    // 2. Gửi gói tin lên Server
    if (NetworkUtils::sendPacket(socket, REQ_HISTORY, builder.getData(), builder.getSize())) {
        cout << ">> [INFO] Da gui yeu cau lay lich su Topic " << topicId << endl;
    } else {
        cout << ">> [ERROR] Khong the gui yeu cau (Mat ket noi?)" << endl;
    }
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
                        cout << "7. Chat Text\t 8. Gui File" << endl;
                        cout << "9. Xem Lich Su (History)" << endl; // <--- Thêm dòng này
                        cout << "0. Dang xuat" << endl;
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
                            case 8: // Thêm case mới để gửi file
                                cout << "--- UPLOAD FILE ---" << endl;
                                cout << "Nhap ID Topic: "; 
                                if (!(cin >> tid)) { cin.clear(); cin.ignore(10000, '\n'); break; }
                                cin.ignore();

                                cout << "Nhap duong dan file (VD: image.png): ";
                                getline(cin, buf);
                                
                                // Gọi hàm uploadFile vừa viết
                                uploadFile(sock, tid, buf);
                                break;
                            case 9: // REQUEST HISTORY ---
                                cout << "Nhap ID Topic muon xem lich su: ";
                                if (!(cin >> tid)) {
                                    cin.clear(); cin.ignore(10000, '\n');
                                    break;
                                }
                                cin.ignore(); // Xóa bộ đệm

                                // Gọi hàm vừa viết
                                requestHistory(sock, tid);
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

