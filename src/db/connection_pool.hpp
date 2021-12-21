#pragma once

#include <string>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <libpq-fe.h>
#include "connection.hpp"

namespace db {

    class connection_pool {
    public:
        connection_pool(int size);
        ~connection_pool();
        void run(const connect_param_t& params);
        void stop();
        
        void async_query(query&& query);
        
    private:
        void loop(const connect_param_t& params);
        std::mutex _mtx_queue;
        std::list<query> _queue;
        int _pipefd[2];
        std::thread _thr;
        int _size;
    };
}
