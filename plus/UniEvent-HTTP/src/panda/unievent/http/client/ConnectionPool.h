#pragma once

#include <string>
#include <cstdint>
#include <unordered_map>
#include <set>

#include <panda/refcnt.h>
#include <panda/string.h>

#include <panda/unievent/Debug.h>

#include "../common/Defines.h"
#include "../common/HostAndPort.h"
#include "../common/Error.h"

namespace panda { namespace unievent { namespace http { namespace client {

template <class C>
class ConnectionPool : public virtual Refcnt {
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
    static constexpr uint64_t DEFAULT_POOL_TIMEOUT = 10000; // [ms]

    ~ConnectionPool() {
        _EDTOR();
    }

    ConnectionPool(Loop* loop = Loop::default_loop(), uint64_t timeout=DEFAULT_POOL_TIMEOUT) : loop_(loop), timeout_(timeout){
        _ECTOR();
    }

    ConnectionSP get(const string& host, uint16_t port) {
        _EDEBUGTHIS("get: %.*s:%d, pool: %zu", (int)host.size(), host.data(), port, connections_.size());

        auto key = Key{host, port};
        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            // no connections to host, create a busy one
            _EDEBUGTHIS("get: no connections, create");
            auto connection = make_iptr<C>(HostAndPort{host, port}, loop_, this, timeout_);
            connections_.emplace(key, Value{{}, {connection}});
            return connection;
        }

        if(pos->second.free.empty()) {
            _EDEBUGTHIS("get: all busy, create");

            //XXX: limit connections?

            // create connection
            auto connection = make_iptr<C>(HostAndPort{host, port}, loop_, this, timeout_);
            pos->second.busy.insert(connection);
            return connection;
        }

        // move connection from free to busy
        _EDEBUGTHIS("get: pick from free");
        auto free_pos = std::begin(pos->second.free);
        auto connection = *free_pos;
        pos->second.free.erase(free_pos);
        pos->second.busy.insert(connection);

        return connection;
    }

    void detach(ConnectionSP connection) {
        _EDEBUGTHIS("detach: pool: %zu", connections_.size());
        auto key = connection->get_host_and_port();
        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            _EDEBUGTHIS("detach: not in pool: %.*s:%d", (int)key.host.size(), key.host.data(), key.port);
            return;
        }

        auto busy_pos = pos->second.busy.find(connection);
        if(busy_pos == std::end(pos->second.busy)) {
            _EDEBUGTHIS("detach: not in pool: %.*s:%d", (int)key.host.size(), key.host.data(), key.port);
            return;
        }

        pos->second.busy.erase(busy_pos);
        pos->second.free.insert(connection);
    }

    void erase(ConnectionSP connection) {
        auto key = connection->get_host_and_port();

        _EDEBUGTHIS("erase: pool: %zu", connections_.size());

        auto pos = connections_.find(key);
        if(pos == std::end(connections_)) {
            _EDEBUGTHIS("erase: not in pool: %.*s:%d", (int)key.host.size(), key.host.data(), key.port);
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

    LoopSP loop() const { return loop_; }

    void loop(LoopSP loop) {
        if(!connections_.empty()) {
            throw PoolError("Pool is not empty");
        }

        loop_ = loop;
    }

    bool empty() const {
        return connections_.empty();
    }

    void clear() {
        connections_.clear();
    }

private:
    LoopSP loop_;
    uint64_t timeout_;
    std::unordered_map<Key, Value, Hash> connections_;
};

}}}} // namespace panda::unievent::http::client
