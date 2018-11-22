#include "Listener.h"

#include <panda/log.h>

namespace panda { namespace unievent { namespace http { namespace server {

// initializing TCP with two params ctor: this way it will pre-create socket so we could set its options in run()
Listener::Listener (Loop* loop, Location& location) : TCP(loop, AF_INET, unievent::use_cached_resolver_by_default), location(location) {
    if (location.ssl_ctx) location.secure = true;
}

void Listener::run() {
    panda_log_info("Listener[run]: listening " << (location.secure ? "https://" : "http://") << location.host << ":" << location.port);

    if(location.reuse_port) {
        int on = 1;
        setsockopt(SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    }

    bind(location.host, location.port);
    listen(location.backlog);
    if (location.ssl_ctx) use_ssl(location.ssl_ctx);
}

}}}} // namespace panda::unievent::http::server
