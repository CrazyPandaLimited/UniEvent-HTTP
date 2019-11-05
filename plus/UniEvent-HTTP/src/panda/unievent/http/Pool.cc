#include "Pool.h"

namespace panda { namespace unievent { namespace http {

static thread_local std::vector<PoolSP> s_instances;
thread_local std::vector<PoolSP>* Pool::_instances = &s_instances;

Pool::Pool (const LoopSP& loop, uint32_t timeout) : _loop(loop), _inactivity_timeout(timeout) {
    _inactivity_timer = new Timer(loop);
    _inactivity_timer->weak(true);
    _inactivity_timer->event.add([this](auto&){ check_inactivity(); });
}

Pool::~Pool () {
    // there might be some clients still active, remove event listener as we no longer care about of those clients
    for (auto& list : _clients) {
        for (auto& client : list.second.busy) client->event_listener(nullptr);
    }
}

ClientSP Pool::get (const NetLoc& netloc) {
    auto pos = _clients.find(netloc);

    if (pos == _clients.end()) { // no clients to host, create a busy one
        ClientSP client = new Client(this);
        _clients.emplace(netloc, NetLocList{{}, {client}});
        return client;
    }

    if (pos->second.free.empty()) { // all clients are busy -> create new client
        //TODO: limit connections?
        ClientSP client = new Client(this);
        pos->second.busy.insert(client);
        return client;
    }

    // move client from free to busy
    auto free_pos = pos->second.free.begin();
    auto client = *free_pos;
    pos->second.free.erase(free_pos);
    pos->second.busy.insert(client);
    return client;
}

void Pool::putback (const ClientSP& client) {
    auto pos = _clients.find(client->last_netloc());
    assert(pos != _clients.end());

    auto busy_pos = pos->second.busy.find(client);
    assert(busy_pos != pos->second.busy.end());

    pos->second.busy.erase(busy_pos);
    pos->second.free.insert(client);
}

void Pool::check_inactivity () {
    auto remove_time = _loop->now() - _inactivity_timeout;
    for (auto it = _clients.begin(); it != _clients.end();) {
        auto& list = it->second.free;
        for (auto it = list.begin(); it != list.end();) {
            if ((*it)->last_activity_time() < remove_time) it = list.erase(it);
            else ++it;
        }
        if (list.empty() && it->second.busy.empty()) it = _clients.erase(it);
        else ++it;
    }
}

}}}