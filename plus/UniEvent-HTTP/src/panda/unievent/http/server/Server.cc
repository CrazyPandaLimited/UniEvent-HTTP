#include "Server.h"

#include <atomic>

#include "../common/Time.h"
#include "../common/Error.h"

using namespace std::placeholders;

namespace panda { namespace unievent { namespace http { namespace server {

std::atomic<uint64_t> Server::lastid(0);

time_t Server::cached_time;

thread_local string Server::cached_time_string_rfc;

thread_local string* Server::cached_time_string_rfc_ptr = &Server::cached_time_string_rfc;

Server::~Server() {
    _EDTOR();
    stop();
}

Server::Server(Loop* loop) :
    running(false),
    _loop(loop),
    _cached_date_timer(make_iptr<Timer>(_loop)) {
    _ECTOR();
    // callback to update http date string every second
    _cached_date_timer->timer_event.add([&](Timer*) {
        cached_time = std::time(0);
        time::datetime dt;
        time::gmtime(cached_time, &dt);
        *cached_time_string_rfc_ptr = rfc822_date(dt);;
        _EDEBUG("ticking timer cache");
    });

    _cached_date_timer->start(1000, 0);
}

const string& Server::http_date_now() const {
    return *cached_time_string_rfc_ptr;
}

void Server::configure(const Config& conf) {
    config_validate(conf);
    bool was_running = running;
    if (was_running)
        stop_listening();
    config_apply(conf);
    if (was_running)
        start_listening();
}

void Server::config_validate(const Config& c) const {
    if (!c.locations.size())
        throw ServerError("no locations to listen supplied");

    for (auto& loc : c.locations) {
        if (!loc.host)
            throw ServerError("empty host in one of locations");
        if (!loc.backlog)
            throw ServerError("zero backlog in one of locations");
        // do not check port, 0 is some free port and you can get if with get_listeners()[i]->get_sockadr();
    }
}

void Server::config_apply(const Config& conf) {
    locations = conf.locations;
}

void Server::run() {
    _EDEBUGTHIS();
    if (running)
        throw ServerError("server already running");
    running = true;
    start_listening();
}

void Server::stop() {
    _EDEBUGTHIS();
    if(!running) {
        return;
    }

    stop_listening();
    connections.clear();
    running = false;
}

ConnectionSP Server::new_connection(uint64_t id) {
    auto connection_ptr = make_iptr<Connection>(this, id);
    connection_ptr->request_callback = request_callback;
    return connection_ptr;
}

void Server::on_connection(ConnectionSP conn) {
    connection_callback(this, conn);
}

void Server::on_remove_connection(ConnectionSP conn) {
    remove_connection_callback(this, conn);
}

void Server::start_listening() {
    for (auto& location : locations) {
        auto l = new Listener(_loop, location);
        l->connection_event.add(std::bind(&Server::on_connect, this, _1, _2, _3));
        l->connection_factory = [this]() { return new_connection(++lastid); };
        l->run();
        listeners.push_back(l);
    }
}

void Server::stop_listening() {
    listeners.clear();
}

void Server::on_connect(Stream* parent, Stream* stream, const CodeError* err) {
    _EDEBUGTHIS();
    if (err) {
        return;
    }

    //if (auto listener = dyn_cast<Listener*>(parent)) {
        //_EDEBUGTHIS("connection");
    //}

    auto connection = dyn_cast<Connection*>(stream);

    connections[connection->id()] = connection;
    connection->eof_event.add(std::bind(&Server::on_disconnect, this, _1));
    connection->run();

    on_connection(connection);

    _EDEBUGTHIS("connected: %zu", connections.size());
}

void Server::on_disconnect(Stream* handle) {
    _EDEBUGTHIS();
    auto conn = dyn_cast<Connection*>(handle);
    remove_connection(conn);
}

void Server::remove_connection(ConnectionSP conn) {
    _EDEBUGTHIS();
    auto erased = connections.erase(conn->id());
    if (!erased) return;
    on_remove_connection(conn);
}

}}}} // namespace panda::unievent::http::server
