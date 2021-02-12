#include "http.h"
#include <xs/function.h>
#include <xs/unievent/Ssl.h>
#include <xs/unievent/Streamer.h>

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
    if ((sv = h.fetch("locations")))              cfg.locations              = xs::in<decltype(cfg.locations)>(sv);
    if ((v  = h.fetch("idle_timeout")))           cfg.idle_timeout           = (double)v * 1000;
    if ((v  = h.fetch("max_headers_size")))       cfg.max_headers_size       = v;
    if ((v  = h.fetch("max_body_size")))          cfg.max_body_size          = v;
    if ((v  = h.fetch("tcp_nodelay")))            cfg.tcp_nodelay            = v.is_true();
    if ((v  = h.fetch("max_keepalive_requests"))) cfg.max_keepalive_requests = v;
}

void fill (Pool::Config& cfg, const Hash& h) {
    Simple v;
    if ((v = h.fetch("timeout"))) cfg.idle_timeout = (double)v * 1000;
    if ((v = h.fetch("max_connections"))) cfg.max_connections = v;
}

void fill (UserAgent::Config& cfg, const Hash& h) {
    Sv sv; Simple v;
    if ((v  = h.fetch("identity")))   cfg.identity = v.as_string();
    if ((sv = h.fetch("ssl_ctx")))    cfg.ssl_ctx  = xs::in<SslContext>(sv);
    if ((sv = h.fetch("proxy")))      cfg.proxy    = xs::in<URISP>(sv);
}

static bool needs_streaming(const Array& arr) noexcept {
    bool even = arr.size() % 2 == 0;
    size_t last = even ? arr.size() - 1 : arr.size() - 2;
    for(size_t i = 0; i < last; i += 2) {
        auto value = arr.at(i + 1);
        if(value.is_array_ref()) {
            Array items(value);
            if (items.size() >= 2) {
                Sv file_content = items[1];
                if (file_content.is_object_ref()) {
                    /*
                    Object obj(file_content);
                    if (obj.isa("UniEvent::Streamer::IInput")) {
                        return true;
                    }
                    */
                    // we do not expect anything else
                    return true;
                }
            }
        }
    }
    return false;
}

static void fill_form_fields(Request* req, Array& arr) {
    using namespace panda;
    bool even = arr.size() % 2 == 0;
    size_t last = even ? arr.size() - 1 : arr.size() - 2;
    auto& form = req->form;
    for(size_t i = 0; i < last; i += 2) {
        string key = arr.at(i).as_string();
        auto value = arr.at(i + 1);
        if (value.is_simple()) {
            form.emplace_back(new FormField(key, value.as_string()));
        }
        else if (value.is_array_ref()) {
            Array items(value);
            if (items.size() >= 2) {
                string mime_type = (items.size() > 2) ? items.at(2).as_string() : "application/octet-stream";
                string filename = items.at(0).as_string();
                auto file_content = items.at(1);
                if (file_content.is_object_ref()) {
                    auto fc = xs::in<panda::unievent::Streamer::IInput*>(file_content);
                    form.emplace_back(new FormFile(key, fc, mime_type, filename));
                }
                else {
                    form.emplace_back(new FormEmbeddedFile(key, file_content.as_string() , mime_type, filename));
                }
            }
            else {
                string err = "incorrect field '";
                err += key;
                err + "'";
                err += "; it should be like '";
                err += key;
                err += "' => [$filename => $content_or_filestream, 'mime/type']";
                throw err;
            }
        } else {
            string err = "incorrect field '";
            err += key;
            err + "'";
            throw err;
        }
    }
}

void fill_form(Request* req, const Sv& sv) {
    bool streaming = false;
    if (sv.is_array_ref())    {
        streaming = needs_streaming(Array(sv));
    } else if(sv.is_hash_ref()) {
        Hash h(sv);
        Sv fields = h.fetch("fields");
        if (fields) streaming = needs_streaming(Array(fields));
    }
    if (!streaming) {
        protocol::http::fill_form(req, sv);
    } else {
        req->form_stream();
        Array arr;
        if (sv.is_array_ref()) {
            arr = sv;
        }
        else {
            Hash h(sv);
            arr = h.fetch("fields");
        }
        fill_form_fields(req, arr);
    }
}


}}}
