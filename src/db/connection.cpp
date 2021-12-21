#include "connection.hpp"
#include "../logger/logger.hpp"
#include <new>
#include <ios>

namespace db {

    std::vector<connection> connection::create(int count, const connect_param_t &params) {
        char** keywords = new char*[params.size() + 1];
        char** values = new char*[params.size() + 1];
        int idx = 0;
        for(auto& p: params) {
            keywords[idx] = (char*)p.first.c_str();
            values[idx] = (char*)p.second.c_str();
            ++idx;
        }
        keywords[idx] = nullptr;
        values[idx] = nullptr;
        
        std::vector<connection> list;
        for (int i = 0; i < count; ++i) {
            PGconn* conn = PQconnectStartParams(keywords, values, 0);
            if (!conn) {
                throw std::bad_alloc();
            }
            if (PQstatus(conn) == CONNECTION_BAD) {
                std::string error = PQerrorMessage(conn);
                PQfinish(conn);
                delete[] keywords;
                delete[] values;
                throw std::ios_base::failure(error);
            }
            list.push_back(connection(conn, i));
        }
        
        delete[] keywords;
        delete[] values;
        return list;
    }

    connection::connection(PGconn* conn, int id) {
        _conn = conn;
        _id = id;
        PQsetnonblocking(_conn, 1);
    }

    connection::connection(connection&& other) {
        _conn = other._conn;
        _id = other._id;
        other._conn = nullptr;
    }

    connection::~connection() {
        if (_conn) {
            PQfinish(_conn);
        }
    }

    PostgresPollingStatusType connection::status() {
        return PQconnectPoll(_conn);
    }

    const char* connection::error() {
        return PQerrorMessage(_conn);
    }
        
    const int& connection::id() const {
        return _id;
    }
    
    const bool& connection::is_busy() const {
        return _is_busy;
    }
    
    int connection::socket() {
        return PQsocket(_conn);
    }
    
    bool connection::reset() {
        return PQresetStart(_conn);
    }

    bool connection::execute(query&& command) {
        _command = std::move(command);
        int retval = 0;
        if (_command.params().size()) {
            char** values = new char*[_command.params().size()];
            int* lengths = new int[_command.params().size()];
            int* formats = new int[_command.params().size()];
            int i = 0;
            for(auto& p: _command.params()) {
                values[i] = (char*)p.data();
                lengths[i] = (int)p.len();
                formats[i] = (int)p.is_binary();
                ++i;
            }
            
            for(int attempt = 1; attempt < 5; ++attempt) {
                retval = PQsendQueryParams(_conn, _command.sql().c_str(),
                                           (int)_command.params().size(),
                                           nullptr, values, lengths, formats, 0);
                if (retval == 1) {
                    break;
                }
                else {
                    log_error("[db] pool[%d] sendQueryParams failed: %s", _id, error());
                }
            }
            delete[] values;
            delete[] lengths;
            delete[] formats;
        }
        else {
            for (int attempt = 1; attempt < 5; ++attempt) {
                retval = PQsendQuery(_conn, _command.sql().c_str());
                if (retval == 1) {
                    break;
                }
                else {
                    log_error("[db] pool[%d] sendQuery failed: %s", _id, error());
                }
            }
        }
        _is_busy = retval;
        _need_flush = _is_busy;
        return _is_busy;
    }

    void connection::consume() {
        
        if (!_is_busy) {
            return;
        }
        
        if (PQconsumeInput(_conn)) {
            flush();
        }
        else {
            log_error("[db] pool[%d] consume failed: %s", _id, PQerrorMessage(_conn));
        }
        
        if (PQisBusy(_conn)) {
            return;
        }
        
        std::list<PGresult*> results;
        while (true) {
            PGresult* res = PQgetResult(_conn);
            if (!res) {
                break;
            }
            results.push_back(res);
        }
        _command.call_handler(results);
        _is_busy = false;
    }
    
    void connection::flush() {
        // After sending any command or data on a nonblocking connection, call PQflush.
        // If it returns 1, wait for the socket to become read- or write-ready.
        // If it becomes write-ready, call PQflush again. If it becomes read-ready, call PQconsumeInput,
        // then call PQflush again. Repeat until PQflush returns 0.
        // (It is necessary to check for read-ready and drain the input with PQconsumeInput,
        // because the server can block trying to send us data, e.g., NOTICE messages,
        // and won't read our data until we read its.)
        // Once PQflush returns 0, wait for the socket to be read-ready
        // and then read the response as described above.
        if (_need_flush) {
            int ret = PQflush(_conn);
            if (ret == 0) {
                _need_flush = false;
            }
            else if (ret == -1) {
                log_error("[db] pool[%d] flush failed: %s", _id, error());
            }
        }
    }

    bool connection::poll_reading() {
        return _is_busy;
    }
    
    bool connection::poll_writing() {
        return _need_flush;
    }
}
