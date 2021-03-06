#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <functional>

namespace spdlog
{
    class logger;
}

namespace dnet
{

class NetPacketInterface;

struct NetConfig
{
    //****************************************************************//
    //****************************必要配置 ****************************//
    //****************************************************************//
    // io 线程的数量， listener线程属于io线程
    uint16_t io_thread_num = 1;
    bool need_listener = true;
    std::string listen_ip = "127.0.0.1";
    uint16_t listen_port = 18889;
    std::string logger_name = "net_logger";
    std::string log_path = "log/net.log";
    uint32_t packet_header_len = 0;

    //****************************************************************//
    //**************************非必要配置 ****************************//
    //****************************************************************//
    //一个连接积压的发送包的数量，超过这个数量自动断开连接
    uint32_t max_unfinished_send_packet = 10;
    //io线程每一次update处理读写事件的数量
    uint32_t io_thread_handle_io_event_num_of_one_update = 50;
    //当io线程update处理的读写事件数量为0时，io线程休眠的时间, 单位microseconds
    uint32_t io_thread_sleep_duration = 1000; // 1ms
    //net_manager每一次update处理的io_event数量
    uint32_t net_manager_handle_io_event_num_of_one_update = 100;
    //epoll_wait每次处理的事件最大数量
    uint32_t epoll_max_event_num = 100;
    //listener监听队列上限
    uint32_t listener_queue_max_num = 10240;


    std::shared_ptr<spdlog::logger> logger;
    typedef std::function<uint32_t (const char *)> GetBodyLen;
    GetBodyLen get_body_len;
};

extern uint64_t g_dnet_time_ms;
}