#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <string>
#include <cstdint>

class ClientHandler {
private:
    int serverSocket;

public:
    ClientHandler(int socket);

    // --- 1. Nhóm Tài Khoản (Có chờ phản hồi) ---
    void requestRegister(const std::string& user, const std::string& pass);
    bool requestLogin(const std::string& user, const std::string& pass);

    // --- 2. Nhóm Topic (Gửi lệnh, kết quả nhận ở ListenerThread) ---
    void requestGetList(bool myTopics);
    void requestCreateTopic(const std::string& name, const std::string& desc);
    void requestDeleteTopic(const std::string& topicName);

    // --- 3. Nhóm Tương tác (Gửi lệnh) ---
    void requestSubscribe(uint32_t topicId);
    void requestUnsubscribe(uint32_t topicId);
    
    // --- 4. Nhóm Gửi Tin ---
    void requestPublish(uint32_t topicId, const std::string& message);
    void requestPublishBinary(uint32_t topicId, const std::string& filePath);

    // --- 5. Nhóm Tra cứu & History ---
    void requestHistory(uint32_t topicId);
    void requestTopicInfo(uint32_t topicId);
    void requestTopicSubs(uint32_t topicId);
};

#endif