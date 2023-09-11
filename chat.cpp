#include <iostream>
#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <fcntl.h>
#include <winioctl.h>


#pragma comment(lib, "Ws2_32.lib")
using namespace std;

#define POLL_SIZE 32

SOCKET  set_nonblock(SOCKET fd) {
    u_long flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_GETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return  ioctlsocket(fd, FIONBIO, &flags);
#endif
};

int main()
{
    WSADATA wsData;

    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        cout << "Error WinSock version initializaion #";
        cout << WSAGetLastError();
        return 1;
    }
    else
        cout << "WinSock initialization is OK" << endl;

    SOCKET MasterSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (MasterSocket == INVALID_SOCKET) {
        cout << "Error initialization socket # " << WSAGetLastError() << endl;
        closesocket(MasterSocket);
        WSACleanup();
        return 1;
    }
    else
        cout << "Server socket initialization is OK" << endl;

    set<SOCKET> SlaveSockets;
    sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(5555);
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(MasterSocket, (struct sockaddr*)(&SockAddr), sizeof(SockAddr)) != 0) {
        cout << "Error Socket binding to server info. Error # " << WSAGetLastError() << endl;
        closesocket(MasterSocket);
        WSACleanup();
        return 1;
    }
    else
        cout << "Binding socket to Server info is OK" << endl;

    set_nonblock(MasterSocket);
    if (listen(MasterSocket, SOMAXCONN) != 0)
    {
        cout << "Can't start to listen to. Error # " << WSAGetLastError() << endl;
        closesocket(MasterSocket);
        WSACleanup();
        return 1;
    }
    else
        cout << "Listening..." << endl;


    pollfd Set[POLL_SIZE];
    Set[0].fd = MasterSocket;
    Set[0].events = POLLIN;
    vector <string> IP;
    IP.push_back("127.0.0.1");

    while (true)
    {
        unsigned int Index = 1;


        for (auto Iter = SlaveSockets.begin(); Iter != SlaveSockets.end(); Iter++) {
            Set[Index].fd = *Iter;
            Set[Index].events = POLLIN;
            Index++;

        }

        unsigned int SetSize = 1 + SlaveSockets.size();
        WSAPoll(Set, SetSize, -1);


        for (unsigned int i = 0; i < SetSize; i++)
        {
            if (Set[i].revents & POLLIN) {
                if (i) {
                    static char Buffer[1024];
                    int RecvSize = recv(Set[i].fd, Buffer, 1024, 0);
                    cout << "Client #" << i << " message: " << Buffer << endl;
                    string mess = IP[i] + ": " + Buffer;
                    if ((RecvSize == 0) && (WSAGetLastError() != EAGAIN))
                    {
                        shutdown(Set[i].fd, SD_BOTH);
                        closesocket(Set[i].fd);
                        SlaveSockets.erase(Set[i].fd);
                    }
                    else if (RecvSize > 0)
                        for (unsigned int j = 1; j < i; j++)
                            if (i != j)
                                send(Set[j].fd, mess.c_str(), mess.size(), 0);
                }
                else {

                    sockaddr_in clientaddr;
                    socklen_t clientaddr_size = sizeof(clientaddr);

                    SOCKET SlaveSocket = accept(MasterSocket, (sockaddr*)&clientaddr, &clientaddr_size);

                    char str[20];
                    inet_ntop(AF_INET, &(clientaddr.sin_addr), str, INET_ADDRSTRLEN);
                    IP.push_back(str);

                    set_nonblock(SlaveSocket);
                    SlaveSockets.insert(SlaveSocket);

                    string mess = IP[i] + " client connect";
                    for (unsigned int j = 1; j < SetSize; j++)
                        send(Set[j].fd, mess.c_str(), mess.size(), 0);
                }

            }
            if (Set[i].revents == 2) {
                string mess = IP[i] + " client disconnect";
                for (unsigned int j = 1; j < SetSize; j++)
                    send(Set[j].fd, mess.c_str(), mess.size(), 0);
            }


        }


    }
    return 0;
}