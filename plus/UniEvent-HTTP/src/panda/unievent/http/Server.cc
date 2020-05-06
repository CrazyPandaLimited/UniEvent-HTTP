#include "Server.h"
#include <ostream>
#include <panda/time.h>

namespace panda { namespace unievent { namespace http {

string rfc822_date (time::ptime_t);

std::atomic<uint64_t> Server::lastid(0);

Server::Server (const LoopSP& loop, IFactory* fac) : _loop(loop), _factory(fac) {}

Server::~Server() {}

void Server::configure (const Config& conf) {
    if (!conf.locations.size()) throw HttpError("no locations to listen supplied");

    for (auto& loc : conf.locations) {
        if (!loc.host) throw HttpError("empty host in one of locations");
    }

    if (_running) stop_listening();

    _conf = conf;
    for (auto& loc : _conf.locations) {
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
    panda_log_notice("stopping HTTP server with " << _connections.size() << " connections");
    while (_connections.size()) _connections.begin()->second->close(errc::server_stopping);
    _running = false;
}

void Server::start_listening () {
    if (_listeners.size()) throw HttpError("server is already listening");
    for (auto& loc : _conf.locations) {
        TcpSP lst = new Tcp(_loop, loc.domain);
        lst->event_listener(this);

        if (loc.reuse_port) {
            int on = 1;
            lst->setsockopt(SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        }

        lst->bind(loc.host, loc.port);
        lst->listen(loc.backlog);
        if (loc.ssl_ctx) lst->use_ssl(loc.ssl_ctx);

        panda_log_notice("listening: " << (loc.ssl_ctx ? "https://" : "http://") << loc.host << ":" << lst->sockaddr().port());
        _listeners.push_back(lst);
    }
}

void Server::stop_listening () {
    _listeners.clear();
}

StreamSP Server::create_connection (const StreamSP&) {
    ServerConnection::Config cfg {_conf.idle_timeout, _conf.max_headers_size, _conf.max_body_size, _factory};
    return new_connection(++lastid, cfg);
}

ServerConnectionSP Server::new_connection (uint64_t id, const ServerConnection::Config& conf) {
    ServerConnectionSP conn = new ServerConnection(this, id, conf);
    if (_conf.tcp_nodelay) conn->set_nodelay(true);
    return conn;
}

void Server::on_connection (const StreamSP& stream, const ErrorCode& err) {
    if (err) return;
    auto connection = dynamic_pointer_cast<ServerConnection>(stream);
    assert(connection);
    _connections[connection->id()] = connection;
    connection->start();
    panda_log_info("client connected to " << connection->sockaddr() << ", id=" << connection->id() << ", total connections: " << _connections.size());
}

const string& Server::date_header_now () {
    if (!_hdate_time || _hdate_time <= _loop->now() - 1000) {
        _hdate_time = _loop->now();
        _hdate_str  = rfc822_date(std::time(0));
    }
    return _hdate_str;
}

const char month_snames[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char day_snames   [7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// adapted from apr_rfc822_date, https://apr.apache.org
string rfc822_date (time::ptime_t epoch) {
    time::datetime dt;
    time::gmtime(epoch, &dt);

    size_t length = sizeof("Mon, 20 Jan 2020 20:20:20 GMT") - 1;
    string result(length);
    result.length(length);
    char* date_str = result.buf();

    const char* s = &day_snames[dt.wday][0];
    *date_str++   = *s++;
    *date_str++   = *s++;
    *date_str++   = *s++;
    *date_str++   = ',';
    *date_str++   = ' ';
    *date_str++   = dt.mday / 10 + '0';
    *date_str++   = dt.mday % 10 + '0';
    *date_str++   = ' ';
    s             = &month_snames[dt.mon][0];
    *date_str++   = *s++;
    *date_str++   = *s++;
    *date_str++   = *s++;
    *date_str++   = ' ';

    *date_str++ = dt.year / 1000 + '0';
    *date_str++ = dt.year % 1000 / 100 + '0';
    *date_str++ = dt.year % 100 / 10 + '0';
    *date_str++ = dt.year % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = dt.hour / 10 + '0';
    *date_str++ = dt.hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = dt.min / 10 + '0';
    *date_str++ = dt.min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = dt.sec / 10 + '0';
    *date_str++ = dt.sec % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = 'G';
    *date_str++ = 'M';
    *date_str++ = 'T';

    return result;
}

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
