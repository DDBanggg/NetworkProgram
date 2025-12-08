# 1. Trình biên dịch và Cờ (Flags)
CXX      = g++
# -pthread: Bắt buộc để dùng std::thread
# -Wall: Hiện cảnh báo để dễ debug
CXXFLAGS = -std=c++11 -pthread -Wall

# 2. Tên file chạy đầu ra (Executable)
CLIENT_BIN = client_app
SERVER_BIN = server_app

# 3. Định nghĩa các file nguồn (Source files)

# Common: Các file dùng chung cho cả 2 bên
COMMON_SRC = common/NetworkUtils.cpp

# Client: Bao gồm Main App + Handler + Common
# [QUAN TRỌNG]: Phải có client/ClientHandler.cpp để sửa lỗi Linker
CLIENT_SRC = client/ClientApp.cpp client/ClientHandler.cpp $(COMMON_SRC)

# Server: Bao gồm Main App + Handler + Common
# [QUAN TRỌNG]: Phải có server/ServerApp.cpp (chứa main)
SERVER_SRC = server/ServerApp.cpp server/ServerHandler.cpp $(COMMON_SRC)

# 4. Quy tắc biên dịch (Targets)

# Gõ 'make' sẽ chạy target này -> build cả Client và Server
all: $(CLIENT_BIN) $(SERVER_BIN)

# Quy tắc build Client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

# Quy tắc build Server
$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

# 5. Lệnh dọn dẹp (Gõ 'make clean' để xóa file cũ đi build lại)
clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)