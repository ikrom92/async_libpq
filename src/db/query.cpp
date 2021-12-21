#include "query.hpp"

namespace db {

    query::query() {}
    
    query::query(query&& other) {
        *this = std::move(other);
    }
    
    query::query(const std::string& sql, callback_t handler) {
        _sql = sql;
        _handler = handler;
    }
    
    query::query(std::string&& sql, callback_t handler) {
        _sql = std::move(sql);
        _handler = handler;
    }
    
    query::query(const std::string& sql, const std::list<param>& params, callback_t handler) {
        _sql = sql;
        _params = params;
        _handler = handler;
    }
    
    query::query(const std::string& sql, std::list<param>&& params, callback_t handler) {
        _sql = sql;
        _params = std::move(params);
        _handler = handler;
    }
    
    query::query(std::string&& sql, std::list<param>&& params, callback_t handler) {
        _sql = std::move(sql);
        _params = std::move(params);
        _handler = handler;
    }
    
    query::~query() {
        call_handler({});
    }
    
    query& query::operator=(query&& other) {
        _sql = std::move(other._sql);
        _params = std::move(other._params);
        _handler = other._handler;
        other._handler = nullptr;
        return *this;
    }
    
    bool query::empty() const {
        return _sql.empty();
    }
    
    const std::string& query::sql() const {
        return _sql;
    }
    
    const std::list<query::param>& query::params() const {
        return _params;
    }
    
    void query::call_handler(const std::list<PGresult*>& results) {
        // Guarantee, that handler will be called once
        if (_handler) {
            _handler(results);
            _handler = nullptr;
        }
    }
    
    bool is_le() {
        static int endiannes = 0; // 1 - bigendiann, 2 - littleendian
        
        if( endiannes == 0 ) {
            short n = 0x0001;
            char* buf = (char*) &n;
            if( buf[0] == 0 ) {
                endiannes = 1;
            }
            else {
                endiannes = 2;
            }
        }
        return endiannes == 2;
    }
    
    
    // query::param
    query::param query::param::boolean(bool boolVal) {
        return param::text(boolVal ? "t" : "f");
    }
    
    query::param query::param::text(const std::string &strVal) {
        return param((void*)strVal.c_str(), strVal.length() + 1, false, true);
    }
    
    query::param query::param::number(void *number, std::size_t size) {
        param p(number, size, true, true);
        if (is_le()) {
            // convert to big endiann
            std::size_t half_size = size / 2;
            for (int i = 0; i < half_size; ++i) {
                std::swap(p._data[i], p._data[size - i - 1]);
            }
        }
        return p;
    }
    
    query::param query::param::int16(int16_t number) {
        return param::number(&number, 2);
    }
    
    query::param query::param::int32(int32_t number) {
        return param::number(&number, 4);
    }
    
    query::param query::param::int64(int64_t number) {
        return param::number(&number, 8);
    }
    
    query::param query::param::uint16(uint16_t number) {
        return param::number(&number, 2);
    }
    
    query::param query::param::uint32(uint32_t number) {
        return param::number(&number, 4);
    }
    
    query::param query::param::uint64(uint64_t number) {
        return param::number(&number, 8);
    }
    
    query::param::param(void* data, std::size_t len, bool binary, bool copy) {
        _len = len;
        _binary = binary;
        if (copy) {
            _data = new unsigned char[_len];
            std::memcpy(_data, data, _len);
            _owner = true;
        }
        else {
            _data = reinterpret_cast<unsigned char*>(data);
            _owner = false;
        }
    }

    query::param::param(const query::param& other) {
        *this = other;
    }

    query::param::param(query::param&& other) {
        *this = std::move(other);
    }
    
    query::param& query::param::operator=(const param& other) {
        _len = other._len;
        _binary = other._binary;
        _owner = other._owner;
        if (_owner) {
            _data = new unsigned char[_len];
            std::memcpy(_data, other._data, _len);
        }
        else {
            _data = other._data;
        }
        return *this;
    }
    
    query::param& query::param::operator=(param&& other) {
        _len = other._len;
        _binary = other._binary;
        _owner = other._owner;
        _data = other._data;
        other._data = nullptr;
        other._len = 0;
        other._owner = false;
        return *this;
    }

    query::param::~param() {
        if (_owner && _data) {
            delete[] _data;
        }
    }

}
