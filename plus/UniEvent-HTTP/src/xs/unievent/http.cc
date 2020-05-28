#include "http.h"
#include <xs/function.h>
#include <xs/unievent/Ssl.h>

namespace xs { namespace unievent { namespace http {

using SslContext = panda::unievent::SslContext;

void fill (Request* req, const Hash& h) {
    xs::protocol::http::fill(req, h);
    Sv sv; Simple v;
    if ((sv = h.fetch("response_callback"))) req->response_event.add(xs::in<Request::response_fn>(sv));
    if ((sv = h.fetch("redirect_callback"))) req->redirect_event.add(xs::in<Request::redirect_fn>(sv));
    if ((sv = h.fetch("partial_callback")))  req->partial_event.add(xs::in<Request::partial_fn>(sv));
    if ((sv = h.fetch("continue_callback"))) req->continue_event.add(xs::in<Request::continue_fn>(sv));
    if ((v  = h.fetch("timeout")))           req->timeout = (double)v * 1000;
    if ((v  = h.fetch("follow_redirect")))   req->follow_redirect = v;
    if ((v  = h.fetch("tcp_nodelay")))       req->tcp_nodelay = v.is_true();
    if ((v  = h.fetch("redirection_limit"))) req->redirection_limit = v;
    if ((sv = h.fetch("ssl_ctx")))           req->ssl_ctx = xs::in<SslContext>(sv);
    if ((sv = h.fetch("proxy")))             req->proxy = xs::in<URISP>(sv);
    if ((sv = h.fetch("tcp_hints")))         req->tcp_hints = xs::in<AddrInfoHints>(sv);
}

void fill (ServerResponse* res, const Hash& h) {
    xs::protocol::http::fill(res, h);
}

void fill (Server::Location& loc, const Hash& h) {
    Sv sv; Simple v;
    if ((v  = h.fetch("host")))       loc.host       = v.as_string();
    if ((v  = h.fetch("port")))       loc.port       = v;
    if ((v  = h.fetch("reuse_port"))) loc.reuse_port = v;
    if ((v  = h.fetch("backlog")))    loc.backlog    = v;
    if ((v  = h.fetch("domain")))     loc.domain     = v;
    if ((sv = h.fetch("ssl_ctx")))    loc.ssl_ctx    = xs::in<SslContext>(sv);
}

void fill (Server::Config& cfg, const Hash& h) {
    Sv sv; Simple v;
    if ((sv = h.fetch("locations")))        cfg.locations        = xs::in<decltype(cfg.locations)>(sv);
    if ((v  = h.fetch("idle_timeout")))     cfg.idle_timeout     = (double)v * 1000;
    if ((v  = h.fetch("max_headers_size"))) cfg.max_headers_size = v;
    if ((v  = h.fetch("max_body_size")))    cfg.max_body_size    = v;
    if ((v  = h.fetch("tcp_nodelay")))      cfg.tcp_nodelay      = v.is_true();
}

void fill (Pool::Config& cfg, const Hash& h) {
    Simple v;
    if ((v = h.fetch("timeout"))) cfg.idle_timeout = (double)v * 1000;
}

void fill (UserAgent::Config& cfg, const Hash& h) {
    Sv sv; Simple v;
    if ((v  = h.fetch("identity")))   cfg.identity = v.as_string();
    if ((sv = h.fetch("ssl_ctx")))    cfg.ssl_ctx  = xs::in<SslContext>(sv);
    if ((sv = h.fetch("proxy")))      cfg.proxy    = xs::in<URISP>(sv);
}


}}}
