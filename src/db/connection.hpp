#pragma once

#include <vector>
#include <map>
#include <string>
#include "query.hpp"

namespace db {
    
    using connect_param_t = std::map<std::string, std::string>;
    
    class connection {
        connection(PGconn* conn, int id);
    public:
        connection() = delete;
        connection(const connection&) = delete;
        connection(connection&& other);
        ~connection();
        
        static std::vector<connection> create(int count, const connect_param_t& param);
        
        const char* error();
        PostgresPollingStatusType status();
        const int& id() const;
        const bool& is_busy() const;
        int socket();
        bool reset();
        bool execute(query&& command);
        void consume();
        void flush();
        
        bool poll_reading();
        bool poll_writing();
        
        
    private:
        int _id;
        PGconn* _conn;
        query _command;
        bool _is_busy = false;
        bool _need_flush = false;
    };
}
