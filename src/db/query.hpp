#pragma once

#include <cstdint>
#include <string>
#include <list>
#include <system_error>
#include <libpq-fe.h>

namespace db {
    
    class query {
    public:
        class param;
        using callback_t = std::function<void(std::list<PGresult*>)>;
        
        query();
        query(query&& other);
        query(const query& other) = delete;
        query(const std::string& sql, callback_t handler);
        query(std::string&& sql, callback_t handler);
        query(const std::string& sql, const std::list<param>& params, callback_t handler);
        query(const std::string& sql, std::list<param>&& params, callback_t handler);
        query(std::string&& sql, std::list<param>&& params, callback_t handler);
        ~query();
        
        query& operator=(const query& other) = delete;
        query& operator=(query&& other);
        
        bool empty() const;
        const std::string& sql() const;
        const std::list<param>& params() const;
        void call_handler(const std::list<PGresult*>& results);
        
    private:
        std::string _sql;
        std::list<param> _params;
        callback_t _handler;
    };

    class query::param {
    public:
        param(void* data, std::size_t len, bool binary, bool copy = false);
        param(const param& other);
        param(param&& other);
        ~param();
        
        param& operator=(const param& other);
        param& operator=(param&& other);
        
        static param boolean(bool boolVal);
        static param text(const std::string& strVal);
        static param number(void* number, std::size_t size);
        static param int16(int16_t number);
        static param int32(int32_t number);
        static param int64(int64_t number);
        static param uint16(uint16_t number);
        static param uint32(uint32_t number);
        static param uint64(uint64_t number);
        
        const void* data() const {
            return _data;
        }
        std::size_t len() const {
            return _len;
        }
        bool is_binary() const {
            return _binary;
        }
        
    private:
        unsigned char* _data = nullptr;
        std::size_t _len = 0;
        bool _binary = false;
        bool _owner = false;
    };

}
