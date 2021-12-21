#include "connection_pool.hpp"
#include "connection.hpp"
#include "../logger/logger.hpp"
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <vector>


namespace db {
    
    namespace command {
        char stop = '0';
        char new_query = '1';
    };
    
    connection_pool::connection_pool(int size): _size(size) {
        _pipefd[0] = _pipefd[1] = -1;
        if (pipe(_pipefd) == -1) {
            log_error("[db] failed to create pipe");
            throw std::runtime_error("failed to create connection pool");
        }
    }
    
    connection_pool::~connection_pool() {
        if (_pipefd[0] != -1) {
            close(_pipefd[0]);
        }
        if (_pipefd[1] != -1) {
            close(_pipefd[1]);
        }
    }

    void connection_pool::async_query(query&& query) {
        std::unique_lock<std::mutex> lock(_mtx_queue);
        bool empty = _queue.empty();
        _queue.push_back(std::move(query));
        lock.unlock();
        
        if (empty) {
            // generate new notification
            write(_pipefd[1], &command::new_query, 1);
        }
    }

    void connection_pool::run(const connect_param_t &params) {
        _thr = std::thread(&connection_pool::loop, this, params);
    }
    
    void connection_pool::stop() {
        write(_pipefd[1], &command::stop, 1);
        _thr.join();
        
        std::unique_lock<std::mutex> lock(_mtx_queue);
        _queue.clear();
    }

    void connection_pool::loop(const connect_param_t& params) {
        
        // Create pool
        std::vector<connection> pool;
        try {
            pool = connection::create(_size, params);
        }
        catch(const std::exception& e) {
            log_error("[db] failed to create connection pool: %s", e.what());
            return;
        }
        
        std::list<query> queries;
        auto clear = [this, &queries] {
            
            for(auto& q: queries) {
                q.call_handler({});
            }
            
            // consume read pipe
            int bytes_available;
            bool has_query = false;
            if(ioctl(_pipefd[0], FIONREAD, &bytes_available) == 0 && bytes_available > 0) {
                char* dummy = new char[bytes_available];
                read(_pipefd[0], dummy, bytes_available);
                delete[] dummy;
            }
            
            std::lock_guard<std::mutex> lock(_mtx_queue);
            for(auto& q: _queue) {
                q.call_handler({});
            }
            _queue.clear();
        };
        
        // Wait for initial connection
        log_info("[db] connection pool is created. waiting for connection");
        while (true) {
            int maxsfd = _pipefd[0];
            fd_set readfds, writefds;
            FD_SET(_pipefd[0], &readfds);
            FD_ZERO(&writefds);
            bool is_connected = true;
            for(auto& c: pool) {
                int sock = c.socket();
                switch (c.status()) {
                    case PGRES_POLLING_OK:
                        log_info("[db] pool[%d] connected", c.id());
                        break;
                    case PGRES_POLLING_FAILED:
                        log_error("[db] pool[%d] failed: %s", c.id(), c.error());
                        clear();
                        return;
                    case PGRES_POLLING_READING:
                        FD_SET(c.socket(), &readfds);
                        is_connected = false;
                        maxsfd = std::max(sock, maxsfd);
                        break;
                    case PGRES_POLLING_WRITING:
                        FD_SET(c.socket(), &writefds);
                        is_connected = false;
                        maxsfd = std::max(sock, maxsfd);
                        break;
                    default:
                        break;
                }
            }
        
            if (is_connected) {
                break;
            }
            else if (select(maxsfd + 1, &readfds, &writefds, nullptr, nullptr) > 0) {
                if (FD_ISSET(_pipefd[0], &readfds)) {
                    int bytes_available;
                    bool has_query = false;
                    if(ioctl(_pipefd[0], FIONREAD, &bytes_available) == 0) {
                        for(int i = 0; i < bytes_available; ++i) {
                            char cmd;
                            read(_pipefd[0], &cmd, 1);
                            if (cmd == command::stop) {
                                log_info("[db] stop called");
                                clear();
                                return;
                            }
                            else if (cmd == command::new_query) {
                                has_query = true;
                            }
                        }
                    }
                    
                    if (has_query) {
                        std::lock_guard<std::mutex> lock(_mtx_queue);
                        for(auto& q: _queue) {
                            queries.push_back(std::move(q));
                        }
                        _queue.clear();
                    }
                }
            }
        }
        log_info("[db] connection pool is connected");
        
        struct timeval tv = {3, 0};
        while (true) {
            fd_set readfds, writefds;
            FD_SET(_pipefd[0], &readfds);
            FD_ZERO(&writefds);
            int maxsfd = _pipefd[0];
            
            // Check connection
            for(auto& c: pool) {
                int sock = c.socket();
                switch (c.status()) {
                    case PGRES_POLLING_OK:
                        if (queries.size() && !c.is_busy()) {
                            c.execute(std::move(queries.front()));
                            queries.pop_front();
                        }
                        break;
                    case PGRES_POLLING_FAILED:
                        log_error("[db] pool[%d] connection aborted: %s", c.id(), c.error());
                        c.reset();
                        FD_SET(sock, &readfds);
                        maxsfd = std::max(maxsfd, sock);
                        break;
                    case PGRES_POLLING_WRITING:
                        FD_SET(sock, &writefds);
                        maxsfd = std::max(maxsfd, sock);
                        break;
                    case PGRES_POLLING_READING:
                        FD_SET(sock, &readfds);
                        maxsfd = std::max(maxsfd, sock);
                        break;
                    default:
                        break;
                }
                
                if (c.poll_reading()) {
                    FD_SET(sock, &readfds);
                    maxsfd = std::max(maxsfd, sock);
                }
                
                if (c.poll_writing()) {
                    FD_SET(sock, &writefds);
                    maxsfd = std::max(maxsfd, sock);
                }
            }
            
            // Wait for data avaiability
            if (select(maxsfd + 1, &readfds, &writefds, NULL, &tv) > 0) {
                
                // handle commands
                if (FD_ISSET(_pipefd[0], &readfds)) {
                    int bytes_available;
                    bool has_query = false;
                    if(ioctl(_pipefd[0], FIONREAD, &bytes_available) == 0) {
                        for(int i = 0; i < bytes_available; ++i) {
                            char cmd;
                            read(_pipefd[0], &cmd, 1);
                            if (cmd == command::stop) {
                                log_info("[db] stop called");
                                clear();
                                return;
                            }
                            else if (cmd == command::new_query) {
                                has_query = true;
                            }
                        }
                    }
                    
                    if (has_query) {
                        std::lock_guard<std::mutex> lock(_mtx_queue);
                        for(auto& q: _queue) {
                            queries.push_back(std::move(q));
                        }
                        _queue.clear();
                    }
                }
                
                // handle queries
                for(auto& p: pool) {
                    int sock = p.socket();
                    if (FD_ISSET(sock, &readfds)) {
                        p.consume();
                    }
                    if (FD_ISSET(sock, &writefds)) {
                        p.flush();
                    }
                }
                
            }
        }
        
    }

}
