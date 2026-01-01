#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFF_SIZE 128
#define PORT 3333

typedef struct {
    SOCKET socket;
    struct sockaddr_in addr;
} ClientInfo;

ClientInfo clients[10];
int client_count = 0;
HANDLE client_mutex;

unsigned __stdcall ClientHandler(void* arg) {
    ClientInfo* client = (ClientInfo*)arg;
    char client_ip[16];
    strcpy(client_ip, inet_ntoa(client->addr.sin_addr));
    int client_port = ntohs(client->addr.sin_port);
    
    char buffer[BUFF_SIZE];
    char message[BUFF_SIZE + 100];
    
    while (1) {
        memset(buffer, 0, BUFF_SIZE);
        int bytes_received = recv(client->socket, buffer, BUFF_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            printf("Client %s:%d disconnected\n", client_ip, client_port);
            break;
        }
        
        buffer[bytes_received] = '\0';
        sprintf(message, "[%s:%d]: %s", client_ip, client_port, buffer);
        
        // 显示消息
        printf("%s", message);
        
        // 广播给其他客户端
        WaitForSingleObject(client_mutex, INFINITE);
        for (int i = 0; i < client_count; i++) {
            if (clients[i].socket != client->socket && clients[i].socket != INVALID_SOCKET) {
                send(clients[i].socket, message, strlen(message), 0);
            }
        }
        ReleaseMutex(client_mutex);
    }
    
    // 从列表移除
    WaitForSingleObject(client_mutex, INFINITE);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client->socket) {
            closesocket(client->socket);
            clients[i].socket = INVALID_SOCKET;
            break;
        }
    }
    ReleaseMutex(client_mutex);
    
    free(client);
    return 0;
}

int main() {
    system("cls");
    printf("Chat Server v1.0\n");
    printf("Port: %d\n", PORT);
    printf("Waiting for clients...\n");
    printf("================================\n\n");
    
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }
    
    // 创建互斥锁
    client_mutex = CreateMutex(NULL, FALSE, NULL);
    
    // 初始化客户端列表
    for (int i = 0; i < 10; i++) {
        clients[i].socket = INVALID_SOCKET;
    }
    
    // 创建服务器socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Failed to create socket\n");
        WSACleanup();
        return 1;
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    // 开始监听
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    
    printf("✓ Server is running on port %d\n\n", PORT);
    
    // 接受客户端连接
    while (1) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == INVALID_SOCKET) {
            continue;
        }
        
        printf("✓ New client: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // 添加到客户端列表
        WaitForSingleObject(client_mutex, INFINITE);
        
        if (client_count >= 10) {
            printf("Maximum clients reached. Connection rejected.\n");
            send(client_socket, "Server is full.\n", 16, 0);
            closesocket(client_socket);
            ReleaseMutex(client_mutex);
            continue;
        }
        
        clients[client_count].socket = client_socket;
        clients[client_count].addr = client_addr;
        client_count++;
        
        ReleaseMutex(client_mutex);
        
        // 创建客户端线程
        ClientInfo* client_info = malloc(sizeof(ClientInfo));
        client_info->socket = client_socket;
        client_info->addr = client_addr;
        
        HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, ClientHandler, client_info, 0, NULL);
        CloseHandle(thread);
        
        // 发送欢迎消息
        char welcome_msg[100];
        sprintf(welcome_msg, "Welcome to chat room! %d user(s) online.\n", client_count);
        send(client_socket, welcome_msg, strlen(welcome_msg), 0);
    }
    
    closesocket(server_socket);
    WSACleanup();
    CloseHandle(client_mutex);
    
    return 0;
}