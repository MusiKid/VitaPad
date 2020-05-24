#include "sock_data.hpp"

#ifdef __linux__
SocketData::SocketData(int fd) : fd(fd), buffer_(128), size_bytes_set(0)
{
    std::async(std::launch::async, [this]() {
        this->device_status = PENDING;
        this->vita_dev = create_device();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        this->device_status = this->vita_dev.dev != NULL && this->vita_dev.sensor_dev != NULL ? CREATED : ERROR;
    });
}
#elif defined(_WIN32)
SocketData::SocketData(SOCKET sock, VIGEM_TARGET_TYPE type) : buffer_(128), size_bytes_set(0), target(NULL)
{
    auto target_handler = [this](PVIGEM_CLIENT _client, PVIGEM_TARGET _target, VIGEM_ERROR res) {
        this->device_status = VIGEM_SUCESS(res) ? CREATED : ERROR;
    };
    this->device_status = PENDING;
    switch (type)
    {
    case Xbox360Wired:
        new_x360_target(&target_handler);
        break;

    default:
        new_ds4_target(&target_handler);
        break;
    };
}
#endif

void SocketData::push_buf_data(const uint8_t *ptr, uint32_t ptr_size)
{
    assert(this->size_fetched());
    auto size = (this->buffer_.size() + ptr_size <= this->size()) ? ptr_size : this->remaining();
    this->buffer_.insert(this->buffer_.end(), ptr, ptr + size);
}

void SocketData::push_size_data(const uint8_t *ptr, uint32_t ptr_size)
{
    auto size = (this->size_bytes_set + ptr_size <= 4) ? ptr_size : 4 - this->size_bytes_set;
    std::memcpy(this->size_buf, ptr, ptr_size);
    this->size_bytes_set += size;
}

SocketData::~SocketData()
{
#ifdef _WIN32
    if (target != NULL && vigem_target_is_attached(target))
        vigem_target_remove(target);
#elif defined(__linux__)
    if (this->vita_dev.dev != NULL)
    {
        libevdev_uinput_destroy(this->vita_dev.dev);
    }
    if (this->vita_dev.sensor_dev != NULL)
    {
        libevdev_uinput_destroy(this->vita_dev.sensor_dev);
    }
#endif
}