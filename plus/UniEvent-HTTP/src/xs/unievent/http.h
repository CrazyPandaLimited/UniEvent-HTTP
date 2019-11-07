#pragma once
#include <xs/function.h>
#include <xs/protocol/http.h>
#include <xs/unievent/Stream.h> // typemap for SSL_CTX*
#include <panda/unievent/http.h>
#include <panda/unievent/http/Server.h>

namespace xs { namespace unievent { namespace http {

using namespace panda::unievent::http;

void fill_request  (const Hash&, Request*);
void fill_response (const Hash&, Response*);

}}}

namespace xs {

template <class TYPE>
struct Typemap<panda::unievent::http::Request*, TYPE> : Typemap<panda::protocol::http::Request*, TYPE> {
    static panda::string_view package () { return "UniEvent::HTTP::Request"; }
};

template <class TYPE>
struct Typemap<panda::unievent::http::RequestSP, panda::iptr<TYPE>> : Typemap<TYPE*> {
    using Super = Typemap<TYPE*>;
    static panda::iptr<TYPE> in (Sv arg) {
        if (!arg.is_hash_ref()) return Super::in(arg);
        panda::iptr<TYPE> ret = make_backref<TYPE>();
        xs::unievent::http::fill_request(arg, ret.get());
        return ret;
    }
};

template <class TYPE>
struct Typemap<panda::unievent::http::Response*, TYPE> : Typemap<panda::protocol::http::Response*, TYPE> {
    static panda::string_view package () { return "UniEvent::HTTP::Response"; }
};

template <class TYPE>
struct Typemap<panda::unievent::http::ResponseSP, panda::iptr<TYPE>> : Typemap<TYPE*> {
    using Super = Typemap<TYPE*>;
    static panda::iptr<TYPE> in (Sv arg) {
        if (!arg.is_hash_ref()) return Super::in(arg);
        panda::iptr<TYPE> ret = make_backref<TYPE>();
        xs::unievent::http::fill_response(arg, ret.get());
        return ret;
    }
};

template <class TYPE>
struct Typemap<panda::unievent::http::Pool*, TYPE> : TypemapObject<panda::unievent::http::Pool*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMG> {
    static panda::string_view package () { return "UniEvent::HTTP::Pool"; }
};



template <class TYPE> struct Typemap<panda::unievent::http::Server::Location, TYPE> : TypemapBase<panda::unievent::http::Server::Location, TYPE> {
    static TYPE in (SV* arg) {
        TYPE loc;
        const Hash h = arg;
        Sv sv; Simple v;

        if ((v  = h.fetch("host")))       loc.host       = v.as_string();
        if ((v  = h.fetch("port")))       loc.port       = v;
        if ((v  = h.fetch("reuse_port"))) loc.reuse_port = v;
        if ((v  = h.fetch("backlog")))    loc.backlog    = v;
        if ((v  = h.fetch("domain")))     loc.domain     = v;
        if ((sv = h.fetch("ssl_ctx")))    loc.ssl_ctx = xs::in<SSL_CTX*>(sv);

        return loc;
    }
};

template <class TYPE> struct Typemap<panda::unievent::http::Server::Config, TYPE> : TypemapBase<panda::unievent::http::Server::Config, TYPE> {
    static TYPE in (SV* arg) {
        TYPE cfg;
        const Hash h = arg;
        Sv sv; Simple v;

        if ((sv = h.fetch("locations")))        cfg.locations        = xs::in<decltype(cfg.locations)>(sv);
        if ((v  = h.fetch("idle_timeout")))     cfg.idle_timeout     = (double)v * 1000;
        if ((v  = h.fetch("max_headers_size"))) cfg.max_headers_size = v;
        if ((v  = h.fetch("max_body_size")))    cfg.max_body_size    = v;

        return cfg;
    }
};

template <class TYPE>
struct Typemap<panda::unievent::http::Server*, TYPE> : TypemapObject<panda::unievent::http::Server*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
    static panda::string_view package () { return "UniEvent::HTTP::Server"; }
};

}
