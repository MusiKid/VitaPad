#ifndef SOCK_DATA_HPP__
#define SOCK_DATA_HPP__
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cstring>

#include <flatbuffers/flatbuffers.h>

#ifdef __linux__
#include <errno.h>
#include <thread>
#include <future>
#include "uinput.h"
#elif defined(_WIN32)
#include "vigem.h"
#endif

enum DeviceStatus {
    NOT_CREATED,
    PENDING,
    CREATED,
    ERROR
};

// Contains all information related to the socket. Also holds the ViGEm `target` on Windows and the devices on Linux.
class SocketData
{
private:
    uint8_t size_buf[4];
    std::vector<uint8_t> buffer_;
    uint8_t size_bytes_set;
public:
#ifdef __linux__
    SocketData(int fd);
    int fd;
    struct vita vita_dev;
#elif defined(_WIN32)
    SocketData(SOCKET sock, VIGEM_TARGET_TYPE type);
    SOCKET socket;
    PVIGEM_TARGET target;
#endif
    ~SocketData();
    DeviceStatus device_status = DeviceStatus::NOT_CREATED;

    const uint8_t *buffer() const
    {
        return this->buffer_.data();
    };
    void push_buf_data(const uint8_t *, uint32_t);
    void clear()
    {
        this->buffer_.clear();
        std::memset(this->size_buf, 0, 4);
        this->size_bytes_set = 0;
    };
    size_t buffer_size() const { return this->buffer_.size(); };

    void push_size_data(const uint8_t *, uint32_t);
    const uint32_t size() const { return flatbuffers::ReadScalar<uint32_t>(this->size_buf); }
    bool size_fetched() const { return this->size_bytes_set == 4; };
    const uint32_t remaining() const
    {
        assert(this->size_fetched());
        return this->size() - this->buffer_.size();
    };
};

#endif