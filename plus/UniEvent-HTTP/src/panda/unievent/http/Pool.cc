#include "Pool.h"
#include <panda/log.h>

namespace panda { namespace unievent { namespace http {

static thread_local struct {
    std::vector<PoolSP> s_instances;
} tls;

thread_local std::vector<PoolSP>* Pool::_instances = &tls.s_instances;

Pool::Pool (const LoopSP& loop, Config cfg) : _loop(loop), _max_connections(cfg.max_connections), _factory(cfg.factory)  {
    idle_timeout(cfg.idle_timeout);
}

Pool::~Pool () {
    // there might be some clients still active, remove event listener as we no longer care about of those clients
    for (auto& list : _clients) {
        for (auto& client : list.second.busy) client->_pool = nullptr;
    }
}

void Pool::idle_timeout (uint32_t val) {
    _idle_timeout = val;
    if (!val) {
        _idle_timer = nullptr;
        return;
    }

    if (_idle_timer) _idle_timer->stop();
    else {
        _idle_timer = new Timer(_loop);
        _idle_timer->weak(true);
        _idle_timer->event.add([this](auto&){ check_inactivity(); });
    }

    _idle_timer->start(val >= 1000 ? 1000 : val);
}

ClientSP Pool::request (const RequestSP& req) {
    req->check();
    ClientSP client;

    auto netloc = req->netloc();
    auto it = _clients.find(netloc);

    // no clients to host, create a busy one
    if (it == _clients.end()) {
        client = new_client();
        _clients.emplace(netloc, NetLocList{{}, {client}, {}});
    }
    // reuse client from free to busy
    else if (!it->second.free.empty()) {
        auto free_pos = it->second.free.begin();
        client = *free_pos;
        it->second.free.erase(free_pos);
        it->second.busy.insert(client);
    }
    // all clients are busy -> create new client, if limit is not hit
    else if (it->second.busy.size() < _max_connections) {
        client = new_client();
        it->second.busy.insert(client);
    }
    // just enqueue the request
    else {
        it->second.queue.emplace_back(req);
    }

    if (client) { client->request(req); }

    return client;
}


void Pool::putback (const ClientSP& client) {
    auto it = _clients.find(client->last_netloc());
    assert(it != _clients.end());
    auto& queue = it->second.queue;
    // process the next requests (if any) on the same client
    if (queue.size()) {
        auto req = queue.front();
        queue.pop_front();
        client->request(req);
    }
    // mark connection as free
    else {
        auto busy_pos = it->second.busy.find(client);
        assert(busy_pos != it->second.busy.end());

        it->second.busy.erase(busy_pos);
        it->second.free.insert(client);
    }
}

void Pool::check_inactivity () {
    panda_mlog_debug(uewslog, "check_inactivity now = " << _loop->now() << " tmt = " << _idle_timeout);
    auto remove_time = _loop->now() - _idle_timeout;
    for (auto it = _clients.begin(); it != _clients.end();) {
        auto& list = it->second.free;
        for (auto it = list.begin(); it != list.end();) {
            if ((*it)->last_activity_time() < remove_time) {
                panda_mlog_debug(uewslog, "removing inactive connection last activity = " << (*it)->last_activity_time());
                it = list.erase(it);
            }
            else ++it;
        }
        if (list.empty() && it->second.busy.empty()) it = _clients.erase(it);
        else ++it;
    }
}

size_t Pool::size () const {
    size_t ret = 0;
    for (auto& row : _clients) {
        ret += row.second.free.size();
        ret += row.second.busy.size();
    }
    return ret;
}

size_t Pool::nbusy () const {
    size_t ret = 0;
    for (auto& row : _clients) ret += row.second.busy.size();
    return ret;
}

}}}
