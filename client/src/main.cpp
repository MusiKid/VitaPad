#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <mutex>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <linux/uinput.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "ctrl.hpp"
#include "sock_data.hpp"
#include "tray.hpp"

#define MAX_EPOLL_EVENTS 10

#elif defined _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <windows.h>
#include <map>

#include "vigem.h"
#endif

#include <common.h>
#include <pad.fbs.hpp>

#define PACKET_SIZE_LIMIT 1024

bool quit = false;
std::mutex quit_lock;

void check_quit(std::thread& tray)
{
    std::lock_guard<std::mutex> lock(quit_lock);
    if (quit)
    {
        tray.join();
        exit(EXIT_SUCCESS);
    }
}

#ifdef __linux__
#define die(str)                   \
    printf("%s : ", __FUNCTION__); \
    puts(str);                     \
    exit(EXIT_FAILURE);
#endif

#ifdef _WIN32
void cleanup(void)
{
    vigem_cleanup();
    WSACleanup();
}
#endif

int main(int argc, char *argv[])
{
#ifdef __linux__
    struct vita vita_dev = create_device();
    if (vita_dev.dev == NULL || vita_dev.sensor_dev == NULL)
    {
        die("uinput error");
    }
#elif defined(_WIN32)
    WSADATA WSAData;
    WSAStartup(MAKEWORD(2, 0), &WSAData);
    atexit(cleanup);

    int res = init_vigem();
    //TODO: manage errors
    if (res == -1)
    {
        puts("Error during device creation");
        exit(EXIT_FAILURE);
    }
#endif
    if (argc < 2)
    {
        puts("missing ip argument");
        exit(EXIT_FAILURE);
    }
    char *ip = argv[1];
#ifdef __linux__
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
        puts("socket error");
        exit(EXIT_FAILURE);
    }

#elif defined(_WIN32)
    SOCKET sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
    {
        puts("socket error");
        exit(EXIT_FAILURE);
    }
#endif

#ifdef __linux__
    struct sockaddr_in vita;
#elif defined _WIN32
    SOCKADDR_IN vita;
#endif
    vita.sin_addr.s_addr = inet_addr(ip);
    vita.sin_family = AF_INET;
    vita.sin_port = htons(NET_PORT);

#ifdef __linux__
    if (connect(sock, (struct sockaddr *)&vita, sizeof(struct sockaddr)) == -1)
    {
        die("Failed to connect");
    }
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags >= 0)
    {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#elif defined(_WIN32)
    if (connect(sock, (SOCKADDR *)&vita, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        puts("connect error");
        exit(EXIT_FAILURE);
    }
#endif

    std::thread tray_thread(&tray_thread_fn);

#ifdef __linux__
    int epoll_fd = epoll_create1(0), n;
    struct epoll_event ev = {0};
    ev.data.ptr = new SocketData(sock);
    ev.events = EPOLLIN | EPOLLRDHUP;
    struct epoll_event *events = new struct epoll_event[MAX_EPOLL_EVENTS];
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ((SocketData *)ev.data.ptr)->fd, &ev);

    while ((n = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, 50)) != -1)
    {
        check_quit(tray_thread);

        for (uint8_t i = 0; i < n; i++)
        {
            auto data = (SocketData *)events[i].data.ptr;
            if (events[i].events & EPOLLRDHUP)
            {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->fd, NULL);
                delete (SocketData *)events[i].data.ptr;
                // puts("Connection closed");
                // exit(EXIT_SUCCESS);
            }

            else if (events[i].events & EPOLLOUT)
            {
                //TODO:
            }

            else if (events[i].events & EPOLLIN)
            {
                if (!data->size_fetched())
                {
                    uint8_t buf[4];
                    int received = recv(data->fd, buf, sizeof(flatbuffers::uoffset_t), 0);
                    if (received == -1)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                            continue;
                        else
                        {
                            puts(strerror(errno));
                            die("Socket error");
                        }
                    }

                    data->push_size_data(buf, received);
                }

                if (data->size_fetched())
                {
                    // Probably an error, clear the memory
                    if (data->size() == 0 || data->size() >= PACKET_SIZE_LIMIT)
                    {
                        data->clear();
                    }
                    else
                    {
                        std::vector<uint8_t> buffer(data->remaining());
                        int received;
                        while ((received = recv(data->fd, buffer.data(), data->remaining(), 0)) > 0)
                        {
                            data->push_buf_data(buffer.data(), received);
                        }
                    }
                }
            }

            if (data->size_fetched() && data->remaining() == 0 &&
                data->device_status == DeviceStatus::CREATED)
            {
                auto verifier = flatbuffers::Verifier(data->buffer(), data->size());
                if (Pad::VerifyMainPacketBuffer(verifier))
                {
                    auto pad_data = Pad::GetMainPacket(data->buffer());
                    emit_pad_data(pad_data, data->vita_dev);
                }
                data->clear();
            }
        }
    }
#elif defined(_WIN32)
    //TODO: Support for multiple devices
    fd_set rfds, wfds, efds;

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    // FD_ZERO(&wfds);
    // FD_SET(sock, &wfds);

    // FD_ZERO(&efds);
    // FD_SET(sock, &efds);

    std::vector<SocketData> socks_data;
    //TODO: config for x360/ds4
    socks_data.push_back(sock, SocketData(sock, DualShock4Wired));

    TIMEVAL timeval;
    timeval.tv_sec = 0;
    timeval.tv_usec = 50 * 10E3;

    while ((n = select(1, &rfds, NULL, NULL, &timeval)) != SOCKET_ERROR)
    {
        check_quit(tray_thread);

        for (size_t i = 0; n > 0 && i < socks_data.size(); i++)
        {
            SocketData data = socks_data[i];
            SOCKET fd = data.socket;

            if (FD_ISSET(fd, &rfds))
            {
                n--;
                if (!data.size_fetched())
                {
                    uint8_t buf[4];
                    int received = recv(fd, buf, sizeof(flatbuffers::uoffset_t), 0);
                    if (received == -1)
                    {
                        if (WSAGetLastError() == EWOULDBLOCK || WSAGetLastError() == EAGAIN)
                            continue;
                    }
                    if (received == 0)
                        continue;

                    data.push_size_data(buf, received);
                }

                if (data.size_fetched())
                {
                    // Probably an error, clear the memory
                    if (data.size() == 0 || data.size() >= PACKET_SIZE_LIMIT)
                    {
                        data.clear();
                    }
                    else
                    {
                        std::vector<uint8_t> buffer(data->remaining());
                        int received;
                        while ((received = recv(data->fd, buffer.data(), data->remaining(), 0)) > 0)
                        {
                            data->push_buf_data(buffer.data(), received);
                        }
                    }
                }
            }

            if (data.size_fetched() && data.remaining() == 0 &&
                data->device_status == DeviceStatus::CREATED)
            {
                auto verifier = flatbuffers::Verifier(data.buffer(), data.size());
                if (Pad::VerifyMainPacketBuffer(verifier))
                {
                    auto pad_data = Pad::GetMainPacket(data.buffer());
                    emit_pad_data(pad_data, data.target);
                }
                data.clear();
            }
        }
    }
}
#endif

    return EXIT_SUCCESS;
}

static void quit_cb(struct tray_menu *item)
{
    (void)item;
    std::lock_guard<std::mutex> lock(quit_lock);
    quit = true;
    tray_exit();
}

static struct tray tray = {
    .icon = (char *)TRAY_ICON,
#if TRAY_WINAPI
    .tooltip = "VitaPad",
#endif
    .menu = (struct tray_menu[]){
        {.text = (char *)"Quit", .cb = quit_cb},
        {.text = NULL}}};

void tray_thread_fn(void)
{
    if (tray_init(&tray) < 0)
    {
        puts("tray error");
    }

    while (tray_loop(1) == 0)
        ;

    return;
}