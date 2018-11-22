#pragma once

#include <ostream>
#include <cstdint>

#include <panda/string.h>

namespace panda { namespace unievent { namespace http { namespace server {

struct Location {
    string   host;
    uint16_t port       = 0;
    bool     secure     = false;
    bool     reuse_port = true; // several listeners(servers) can be bound to the same port if true, useful for threaded apps
    int      backlog    = 4096; // max accept queue
    SSL_CTX* ssl_ctx    = nullptr;
};

inline
std::ostream& operator<<(std::ostream& os, const Location& location) {
    os << "Location{";
    os << "host:\"" << location.host << "\"";
    os << ",port:" << location.port;
    os << ",secure:" << location.secure;
    os << ",reuse_port:" << location.reuse_port;
    os << ",backlog:" << location.backlog;
    os << "}";
    return os;
}

}}}} // namespace panda::unievent::http::server
