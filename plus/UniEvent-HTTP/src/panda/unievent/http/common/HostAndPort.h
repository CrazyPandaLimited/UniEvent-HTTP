#pragma once

#include <ostream>
#include <cstdint>

#include <panda/string.h>
#include <panda/lib.h>

namespace panda { namespace unievent { namespace http {

struct HostAndPort {
    bool operator==(const HostAndPort &other) const { return host == other.host && port == other.port; }

    string to_string() const { return host + ":" + panda::to_string(port); }

    string host;
    uint16_t port;
};

inline
std::ostream& operator<<(std::ostream& os, const HostAndPort& h) {
    os << h.to_string();
    return os;
}

}}} // namespace panda::unievent::http
