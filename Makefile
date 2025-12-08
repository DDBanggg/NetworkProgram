# 1. Trình biên dịch và Cờ (Flags)
CXX      = g++
CXXFLAGS = -std=c++11 -pthread -Wall

# 2. Tên file chạy đầu ra (Executable)
CLIENT_BIN = client_app
SERVER_BIN = server_app

# 3. Định nghĩa các file nguồn (Source files)
# Common: Các file dùng chung
COMMON_SRC = common/NetworkUtils.cpp

# Client: File main của client + Common
CLIENT_SRC = client/ClientApp.cpp client/ClientHandler.cpp $(COMMON_SRC)

# Server: File main của server + Handler + Common
# Lưu ý: Bạn cần có file server/ServerApp.cpp chứa hàm main()
SERVER_SRC = server/ServerApp.cpp server/ServerHandler.cpp $(COMMON_SRC)

# 4. Quy tắc biên dịch (Targets)

# Gõ 'make' sẽ chạy cái này -> build cả 2
all: $(CLIENT_BIN) $(SERVER_BIN)

# Quy tắc build Client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

# Quy tắc build Server
$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

# 5. Lệnh dọn dẹp (Gõ 'make clean')
clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)