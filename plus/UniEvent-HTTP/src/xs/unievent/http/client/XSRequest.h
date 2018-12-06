#pragma once
#include "error.h"
#include <xs/unievent/XSCallback.h>
#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/uri.h>
#include <panda/unievent.h>
#include <panda/unievent/http.h>

namespace xs { namespace unievent { namespace http { namespace client {

using xs::my_perl;
using panda::string;
using panda::iptr;
using panda::uri::URI;
using panda::unievent::LoopSP;

using namespace panda::unievent::http::client;

namespace proto = panda::protocol::http;

struct XSRequest : Request {
    XSCallback xs_response_cb;
    XSCallback xs_redirect_cb;
    XSCallback xs_error_cb;

    XSRequest(proto::Request::Method method, 
        iptr<URI> uri, 
        proto::HeaderSP header, 
        proto::BodySP body, 
        const string& http_version,
        SV* response_c,
        SV* redirect_c,
        SV* error_c,
        LoopSP loop,
        uint64_t timeout,
        uint8_t redirection_limit) : 
        Request(method, 
                uri, 
                header, 
                body, 
                http_version, 
                response_c ? response_cb : ResponseCallback(nullptr), 
                redirect_c ? redirect_cb : RedirectCallback(nullptr), 
                error_c ? error_cb : ErrorCallback(nullptr), 
                loop, 
                timeout, 
                redirection_limit) {
        xs_response_cb.set(response_c);
        xs_redirect_cb.set(redirect_c);
        xs_error_cb.set(error_c);
    }

    struct Builder : BasicBuilder<Builder> {
        Builder& response_callback (SV* xs_callback) { xs_response_callback_ = xs_callback; return *this; }
        Builder& redirect_callback (SV* xs_callback) { xs_redirect_callback_ = xs_callback; return *this; }
        Builder& error_callback (SV* xs_callback) { xs_error_callback_ = xs_callback; return *this; }

        XSRequest* build () {
            return new XSRequest(method_, uri_, header_, body_, http_version_, xs_response_callback_, xs_redirect_callback_, xs_error_callback_, loop_, timeout_, redirection_limit_);
        }

    private:
        SV* xs_response_callback_ = nullptr;
        SV* xs_redirect_callback_ = nullptr;
        SV* xs_error_callback_ = nullptr;
    };

private:
    static void response_cb (RequestSP, ResponseSP);
    static void redirect_cb (RequestSP, iptr<URI>);
    static void error_cb (RequestSP, const string&);
};

}}}} // namespace xs::unievent::http::client

namespace xs {
    template <class TYPE> struct Typemap<panda::unievent::http::client::Request*, TYPE> : TypemapObject<panda::unievent::http::client::Request*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
        std::string package () { return "UniEvent::HTTP::Request"; }
    };
    
    template <class TYPE> struct Typemap<panda::unievent::http::client::Response*, TYPE> : TypemapObject<panda::unievent::http::client::Response*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
        std::string package () { return "UniEvent::HTTP::Response"; }
    };
} // namespace xs