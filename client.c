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

SOCKET g_sock;
int g_running = 1;

// 接收消息的线程
unsigned __stdcall ReceiveThread(void* arg) {
    char buffer[BUFF_SIZE];
    
    while (g_running) {
        memset(buffer, 0, BUFF_SIZE);
        int bytes_received = recv(g_sock, buffer, BUFF_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("%s", buffer);
            printf(">> ");
            fflush(stdout);
        } else if (bytes_received == 0) {
            printf("\n[系统] 服务器连接已断开\n");
            g_running = 0;
            break;
        } else if (WSAGetLastError() == WSAECONNRESET) {
            printf("\n[系统] 连接被服务器重置\n");
            g_running = 0;
            break;
        }
        
        // 小延迟避免高CPU占用
        Sleep(10);
    }
    
    return 0;
}

int main() {
    system("cls");
    
    char server_address[256] = "localhost";
    
    printf("================================\n");
    printf("       聊天室客户端\n");
    printf("================================\n\n");
    printf("自动连接到本地服务器 (localhost)\n");
    printf("如果要连接到其他服务器，请使用: client.exe IP地址\n\n");
    
    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[错误] 网络初始化失败\n");
        system("pause");
        return 1;
    }
    
    // 创建socket
    g_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_sock == INVALID_SOCKET) {
        printf("[错误] 创建socket失败\n");
        WSACleanup();
        system("pause");
        return 1;
    }
    
    // 解析主机名
    struct hostent* host = gethostbyname(server_address);
    if (host == NULL) {
        printf("[错误] 无法解析服务器地址: %s\n", server_address);
        printf("请检查服务器是否运行\n");
        closesocket(g_sock);
        WSACleanup();
        system("pause");
        return 1;
    }
    
    // 设置服务器地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = *(unsigned long*)host->h_addr_list[0];
    serverAddr.sin_port = htons(PORT);
    
    // 连接到服务器
    printf("正在连接到服务器 %s:%d...\n", server_address, PORT);
    if (connect(g_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("[错误] 连接服务器失败\n");
        printf("请确保:\n");
        printf("1. 服务器已启动 (运行 server.exe)\n");
        printf("2. 防火墙允许连接端口 %d\n", PORT);
        closesocket(g_sock);
        WSACleanup();
        system("pause");
        return 1;
    }
    
    printf("[系统] 成功连接到服务器\n");
    printf("[系统] 输入 '/quit' 退出聊天室\n");
    printf("================================\n\n");
    printf(">> ");
    
    // 创建接收线程
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, ReceiveThread, NULL, 0, NULL);
    
    char buffer[BUFF_SIZE];
    
    // 主线程处理发送
    while (g_running) {
        if (fgets(buffer, BUFF_SIZE, stdin) == NULL) {
            break;
        }
        
        // 移除换行符
        buffer[strcspn(buffer, "\n")] = '\0';
        
        // 检查退出命令
        if (strcmp(buffer, "/quit") == 0 || strcmp(buffer, "/QUIT") == 0) {
            printf("[系统] 正在断开连接...\n");
            g_running = 0;
            break;
        }
        
        // 如果消息为空，跳过
        if (strlen(buffer) == 0) {
            printf(">> ");
            continue;
        }
        
        // 添加换行符并发送
        strcat(buffer, "\n");
        send(g_sock, buffer, strlen(buffer), 0);
    }
    
    // 等待接收线程结束
    WaitForSingleObject(hThread, 1000);
    
    // 清理
    CloseHandle(hThread);
    shutdown(g_sock, SD_BOTH);
    closesocket(g_sock);
    WSACleanup();
    
    printf("\n[系统] 客户端已退出\n");
    system("pause");
    
    return 0;
}