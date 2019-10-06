//#include "Listener.h"
//
//namespace panda { namespace unievent { namespace http { namespace server {
//
//// initializing TCP with two params ctor: this way it will pre-create socket so we could set its options in run()
//Listener::Listener(Loop* loop, Location& location) : Tcp(loop, AF_INET), location(location) {
//    _ECTOR();
//    if (location.ssl_ctx)
//        location.secure = true;
//}
//
//void Listener::run() {
//    _EDEBUGTHIS("listening: %s%.*s:%d", (location.secure ? "https://" : "http://"), (int)location.host.size(), location.host.c_str(), location.port);
//    if (location.reuse_port) {
//        int on = 1;
//        setsockopt(SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
//    }
//
//    bind(location.host, location.port);
//    listen(location.backlog);
//    if (location.ssl_ctx)
//        use_ssl(location.ssl_ctx);
//}
//
//}}}} // namespace panda::unievent::http::server
