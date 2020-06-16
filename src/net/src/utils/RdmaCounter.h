#pragma once

#include <chrono>
#include <fstream>
#include <iostream>
#include "Config.h"


namespace rdma {
    
class RdmaCounter {
public:
    RdmaCounter(bool scoped = false) : scoped(scoped) {
        if (scoped)
            start();
    }

    ~RdmaCounter() {
        if (scoped)
            stop();
    }

    void start() {
        std::ifstream rdma_recv_counter_file;
        rdma_recv_counter_file.open(Config::RDMA_DEVICE_FILE_PATH+"/ports/1/counters/port_rcv_data");
        std::ifstream rdma_sent_counter_file;
        rdma_sent_counter_file.open(Config::RDMA_DEVICE_FILE_PATH+"/ports/1/counters/port_xmit_data");

        rdma_recv_counter_file >> bytes_recv_start;
        rdma_sent_counter_file >> bytes_sent_start;
        startTime = std::chrono::steady_clock::now();
    }

    void stop() {
        std::ifstream rdma_recv_counter_file;
        rdma_recv_counter_file.open(Config::RDMA_DEVICE_FILE_PATH+"/ports/1/counters/port_rcv_data");
        std::ifstream rdma_sent_counter_file;
        rdma_sent_counter_file.open(Config::RDMA_DEVICE_FILE_PATH+"/ports/1/counters/port_xmit_data");

        rdma_recv_counter_file >> bytes_recv_stop;
        rdma_sent_counter_file >> bytes_sent_stop;

        stopTime = std::chrono::steady_clock::now();

        std::cout << "Elapsed time(s)\tBytes sent(MiB)\tSend bandwidth(MiB/s)\n";
        std::cout << to_string(getDuration()) + "\t" + to_string(getSentBytes()/1024/1024) + "\t" + to_string(getSentBytes()/1024/1024/getDuration()) << std::endl;
    }

    double getDuration() {
        return std::chrono::duration<double>(stopTime - startTime).count();
    }

    double getSentBytes() {
        return (bytes_sent_stop - bytes_sent_start)*4;//4 IB lanes
    }
    double getRecvBytes() {
        return (bytes_recv_stop - bytes_recv_start)*4;//4 IB lanes
    }
private:
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    std::chrono::time_point<std::chrono::steady_clock> stopTime;
    bool scoped = false;
    size_t bytes_sent_start = 0;
    size_t bytes_sent_stop = 0;
    size_t bytes_recv_start = 0;
    size_t bytes_recv_stop = 0;
};

}