#include "Server.h"
#include <ostream>

//
//#include "../common/Time.h"
//#include "../common/Error.h"
//
//using namespace std::placeholders;

namespace panda { namespace unievent { namespace http {

std::atomic<uint64_t> Server::lastid(0);

Server::Server (const LoopSP& loop) : _loop(loop), _running()/*, _cached_date_timer(new Timer(loop))*/ {
//    _cached_date_timer->event.add([this](auto&) {
//        _cached_date.clear();
//    });
//    _cached_date_timer->start(1000);
//    // callback to update http date string every second
//    _cached_date_timer->event.add([&](Timer*) {
//        cached_time = std::time(0);
//        time::datetime dt;
//        time::gmtime(cached_time, &dt);
//        *cached_time_string_rfc_ptr = rfc822_date(dt);;
//        _EDEBUG("ticking timer cache");
//    });
//
//    _cached_date_timer->start(1000, 0);
}

Server::~Server() {
    stop();
}

void Server::configure (const Config& conf) {
    if (!conf.locations.size()) throw HttpError("no locations to listen supplied");

    for (auto& loc : conf.locations) {
        if (!loc.host) throw HttpError("empty host in one of locations");
    }

    if (_running) stop_listening();

    _locations = conf.locations;
    for (auto& loc : _locations) {
        if (!loc.backlog) loc.backlog = DEFAULT_BACKLOG;
    }

    if (_running) start_listening();
}

void Server::run () {
    if (_running) throw HttpError("server is already running");
    _running = true;
    start_listening();
}

void Server::stop () {
    if (!_running) return;
    stop_listening();
    _connections.clear();
    _running = false;
}

void Server::start_listening () {
    for (auto& loc : _locations) {
        TcpSP lst = new Tcp(_loop);
        lst->event_listener(this);

        if (loc.reuse_port) {
            int on = 1;
            lst->setsockopt(SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        }

        lst->bind(loc.host, loc.port);
        lst->listen(loc.backlog);
        if (loc.ssl_ctx) lst->use_ssl(loc.ssl_ctx);

        panda_log_info("listening: " << (loc.ssl_ctx ? "https://" : "http://") << loc.host << ":" << lst->sockaddr().port());
        _listeners.push_back(lst);
    }
}

void Server::stop_listening () {
    _listeners.clear();
}

StreamSP Server::create_connection (const StreamSP&) {
    return new_connection(++lastid);
}

ServerConnectionSP Server::new_connection (uint64_t id) {
    ServerConnectionSP conn = new ServerConnection(this, id);
    //conn->request_callback = request_callback;
    return conn;
}

//const string& Server::http_date_now() const {
//    return *cached_time_string_rfc_ptr;
//}
//



//
//void Server::on_connection(ConnectionSP conn) {
//    connection_callback(this, conn);
//}
//
//void Server::on_remove_connection(ConnectionSP conn) {
//    remove_connection_callback(this, conn);
//}
//

//

//
//void Server::on_connect(const StreamSP&, const StreamSP& stream, const CodeError& err) {
//    _EDEBUGTHIS();
//    if (err) {
//        return;
//    }
//
//    //if (auto listener = dyn_cast<Listener*>(parent)) {
//        //_EDEBUGTHIS("connection listener");
//    //}
//
//    auto connection = dynamic_pointer_cast<Connection>(stream);
//
//    connections[connection->id()] = connection;
//    connection->eof_event.add(std::bind(&Server::on_disconnect, this, _1));
//    connection->run();
//
//    on_connection(connection);
//
//    _EDEBUGTHIS("connected: %zu", connections.size());
//}
//
//void Server::on_disconnect(Stream* handle) {
//    _EDEBUGTHIS();
//    auto conn = dyn_cast<Connection*>(handle);
//    remove_connection(conn);
//}
//
//void Server::remove_connection(ConnectionSP conn) {
//    _EDEBUGTHIS();
//    auto erased = connections.erase(conn->id());
//    if (!erased) return;
//    on_remove_connection(conn);
//}

std::ostream& operator<< (std::ostream& os, const Server::Location& location) {
    os << "Location{";
    os << "host:\"" << location.host << "\"";
    os << ",port:" << location.port;
    os << ",secure:" << bool(location.ssl_ctx);
    os << ",reuse_port:" << location.reuse_port;
    os << ",backlog:" << location.backlog;
    os << "}";
    return os;
}

std::ostream& operator<< (std::ostream& os, const Server::Config& conf) {
    os << "ServerConfig{ locations:[";
    for (auto loc : conf.locations) os << loc << ",";
    os << "]};";
    return os;
}

}}}
