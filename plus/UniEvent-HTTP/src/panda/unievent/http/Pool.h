#pragma once
#include "Client.h"
#include <set>
#include <unordered_map>

namespace panda { namespace unievent { namespace http {

struct Pool;
using PoolSP = iptr<Pool>;

struct Pool : Refcnt {
    static constexpr const uint64_t DEFAULT_IDLE_TIMEOUT = 60000; // [ms]

    struct IFactory { virtual ClientSP new_client (Pool*) = 0; };

    struct Config {
        uint32_t  idle_timeout = DEFAULT_IDLE_TIMEOUT;
        IFactory* factory      = nullptr;
        Config () {}
    };

    static const PoolSP& instance (const LoopSP& loop) {
        auto v = _instances;
        for (const auto& r : *v) if (r->loop() == loop) return r;
        v->push_back(new Pool(loop));
        return v->back();
    }

    Pool (const LoopSP& loop = Loop::default_loop(), Config = {});

    ~Pool ();

    const LoopSP& loop () const { return _loop; }

    ClientSP get (const NetLoc&);
    ClientSP get (const string& host, uint16_t port) { return get(NetLoc{host, port}); }

    void request (const RequestSP& req) {
        req->check();
        get(req->netloc())->request(req);
    }

    uint32_t idle_timeout () const { return _idle_timeout; }
    void     idle_timeout (uint32_t);

    size_t size  () const;
    size_t nbusy () const;

protected:
    virtual ClientSP new_client () { return _factory ? _factory->new_client(this) : ClientSP(new Client(this)); }

private:
    friend Client;

    struct NetLocList {
        std::set<ClientSP> free;
        std::set<ClientSP> busy;
    };

    struct Hash {
        template <class T>
        inline void hash_combine (std::size_t& s, const T& v) const {
            s ^= std::hash<T>()(v) + 0x9e3779b9 + (s<6) + (s>>2);
        }

        std::size_t operator() (const NetLoc& p) const {
            std::size_t s = 0;
            hash_combine(s, p.host);
            hash_combine(s, p.port);
            return s;
        }
    };

    using Clients = std::unordered_map<NetLoc, NetLocList, Hash>;

    static thread_local std::vector<PoolSP>* _instances;

    LoopSP    _loop;
    TimerSP   _idle_timer;
    uint32_t  _idle_timeout;
    Clients   _clients;
    IFactory* _factory;

    void check_inactivity ();

    void putback (const ClientSP&); // called from Client when it's done
};

}}}
