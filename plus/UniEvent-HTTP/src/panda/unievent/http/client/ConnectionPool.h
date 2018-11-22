#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include <set>

#include <panda/log.h>
#include <panda/refcnt.h>
#include <panda/string.h>

#include "../common/Defines.h"
#include "../common/HostAndPort.h"
#include "../common/Error.h"

namespace panda { namespace unievent { namespace http { namespace client {

template <class C>    
class ConnectionPool {
private:
    using ConnectionSP = iptr<C>;
    using Key = HostAndPort;
    struct Value {
        std::set<ConnectionSP> free;
        std::set<ConnectionSP> busy;
    };
    
    struct Hash {
	template <class T>
	inline void hash_combine(std::size_t& s, const T& v) const {
	    std::hash<T> hasher;
	    s ^= hasher(v) + 0x9e3779b9 + (s<6) + (s>>2);
	}

	std::size_t operator()(const Key& p) const {
	    std::size_t s = 0;
	    hash_combine(s, p.host);
	    hash_combine(s, p.port);
	    return s;  
	}
    };

public:
    ~ConnectionPool() { 
        panda_log_debug("dtor, pool size: " << connections_.size());
    } 

    ConnectionPool(uint64_t timeout=5000, Loop* loop = Loop::default_loop()) : timeout_(timeout), loop_(loop) {
        panda_log_info("ctor, default loop = " << (loop == Loop::default_loop()));
    } 

    ConnectionSP get(const string& host, uint16_t port) {
        panda_log_debug("get " << host << ":" << port << " pool size: " << connections_.size());

        auto key = Key{host, port};
        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            // no connections to host, create a busy one
            panda_log_debug("get " << key << " no connections, create new");
            auto connection = make_iptr<C>(HostAndPort{host, port}, loop_, this, timeout_);
            connections_.emplace(key, Value{{}, {connection}});
            return connection;
        }

        if(pos->second.free.empty()) {
            panda_log_debug("get " << key << " all busy, create new");

            //XXX: limit connections?

            // create connection
            auto connection = make_iptr<C>(HostAndPort{host, port}, loop_, this, timeout_);
            pos->second.busy.insert(connection);
            return connection;
        }

        // move connection from free to busy
        panda_log_debug("get " << key << " pick from free connections");
        auto free_pos = std::begin(pos->second.free);
        auto connection = *free_pos;
        pos->second.free.erase(free_pos);
        pos->second.busy.insert(connection);

        return connection;
    }

    void detach(ConnectionSP connection) {
        auto key = connection->get_host_and_port();

        panda_log_debug("release " << key << " pool size: " << connections_.size());

        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            panda_log_warn("connection is not in pool" << key.host << ":" << key.port);
            return;
        }

        auto busy_pos = pos->second.busy.find(connection);
        if(busy_pos == std::end(pos->second.busy)) {
            panda_log_warn("connection is not in pool" << key.host << ":" << key.port);
            return;
        }

        pos->second.busy.erase(busy_pos);
        pos->second.free.insert(connection);
    }
    
    void erase(ConnectionSP connection) {
        auto key = connection->get_host_and_port();

        panda_log_debug("erase " << connection.get() << " "<< key << " pool size: " << connections_.size());

        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            panda_log_warn("connection is not in pool" << key.host << ":" << key.port);
            return;
        }

        auto busy_pos = pos->second.busy.find(connection);
        if(busy_pos != std::end(pos->second.busy)) {
            pos->second.busy.erase(busy_pos);
        }
        
        auto free_pos = pos->second.free.find(connection);
        if(free_pos != std::end(pos->second.free)) {
            pos->second.free.erase(free_pos);
        }

        if(pos->second.busy.empty() && pos->second.free.empty()) {
            connections_.erase(pos);
        }
    }

    iptr<Loop> loop() const { return loop_; }
    
    void loop(iptr<Loop> loop) { 
        if(!connections_.empty()) {
            throw PoolError("Pool is not empty"); 
        }

        loop_ = loop; 
    }

    bool empty() const {
        return connections_.empty();
    }

private:
    uint64_t timeout_;
    iptr<Loop> loop_;
    std::unordered_map<Key, Value, Hash> connections_;
};

}}}} // namespace panda::unievent::http::client
