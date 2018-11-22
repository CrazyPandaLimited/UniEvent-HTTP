#include "Server.h"

#include <atomic>

#include <panda/log.h>

#include "../common/Time.h"
#include "../common/Error.h"

using namespace std::placeholders;

namespace panda { namespace unievent { namespace http { namespace server {
    
std::atomic<uint64_t> Server::lastid(0);

time_t Server::cached_time;

thread_local string Server::cached_time_string_rfc;

thread_local string* Server::cached_time_string_rfc_ptr = &Server::cached_time_string_rfc;

Server::~Server() {
    panda_log_info("dtor");
    stop();
}

Server::Server(Loop* loop) : 
    running(false), 
    _loop(loop), 
    _cached_date_timer(make_iptr<Timer>(_loop)) {
    panda_log_info("ctor, default loop = " << (_loop == Loop::default_loop()));

    // callback to update http date string every second
    _cached_date_timer->timer_event.add([&](Timer*) {
        cached_time = std::time(0);
        time::datetime dt;
        time::gmtime(cached_time, &dt);
        *cached_time_string_rfc_ptr = rfc822_date(dt);;

        panda_log_debug("ticking timer cache: [" << cached_time_string_rfc << "]");
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
    panda_log_info("run");
    if (running)
        throw ServerError("server already running");
    running = true;
    //panda_log_info("websocket::Server::run with conn_conf:" << conn_conf);

    start_listening();
}

void Server::stop() {
    panda_log_info("stop");
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
    if (err) {
        panda_log_info("Server[on_connect]: error: " << err->whats());
        return;
    }

    if (auto listener = dyn_cast<Listener*>(parent)) {
        panda_log_info("Server[on_connect]: somebody connected to " << listener->location);
    }

    auto connection = dyn_cast<Connection*>(stream);

    connections[connection->id()] = connection;
    connection->eof_event.add(std::bind(&Server::on_disconnect, this, _1));
    connection->run();

    on_connection(connection);

    panda_log_info("Server[on_connect]: now i have " << connections.size() << " connections");
}

void Server::on_disconnect(Stream* handle) {
    auto conn = dyn_cast<Connection*>(handle);
    panda_log_info("Server[on_disconnect]: disconnected id " << conn->id());
    remove_connection(conn);
}

void Server::remove_connection(ConnectionSP conn) {
    auto erased = connections.erase(conn->id());
    if (!erased) return;
    on_remove_connection(conn);
    panda_log_info("Server[remove_connection]: now i have " << connections.size() << " connections");
}

}}}} // namespace panda::unievent::http::server
