#pragma once

#include "error.h"

#include <xs.h>

#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/unievent/http/common/Response.h>
#include <panda/uri.h>
#include <panda/unievent.h>
#include <panda/unievent/http.h>

namespace xs { namespace unievent { namespace http { namespace client {

using xs::my_perl;
using panda::string;
using panda::iptr;
using panda::uri::URI;

using namespace panda::unievent::http::client;

namespace proto = panda::protocol::http;

struct XSRequest : client::Request {
    Sub xs_response_cb;
    Sub xs_redirect_cb;
    Sub xs_error_cb;

    using ResponseSP = panda::unievent::http::ResponseSP;

    XSRequest(proto::Request::Method method,
        iptr<URI> uri,
        proto::Header&& header,
        proto::BodySP body,
        const string& http_version,
        SV* response_c,
        SV* redirect_c,
        SV* error_c,
        uint64_t timeout,
        uint8_t redirection_limit) :
        Request(method,
                uri,
                std::move(header),
                body,
                http_version,
                response_c ? response_cb : ResponseCallback(nullptr),
                redirect_c ? redirect_cb : RedirectCallback(nullptr),
                error_c ? error_cb : ErrorCallback(nullptr),
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
            return new XSRequest(method_, uri_, std::move(header_), body_, http_version_, xs_response_callback_, xs_redirect_callback_, xs_error_callback_, timeout_, redirection_limit_);
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
        static std::string package () { return "UniEvent::HTTP::Request"; }
    };

    template <class TYPE> struct Typemap<panda::unievent::http::Response*, TYPE> : TypemapObject<panda::unievent::http::Response*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
        static std::string package () { return "UniEvent::HTTP::Response"; }
    };

    template <class TYPE> struct Typemap<panda::unievent::http::client::Connection*, TYPE> : TypemapObject<panda::unievent::http::client::Connection*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
        static std::string package () { return "UniEvent::HTTP::Connection"; }
    };

    template <class TYPE> struct Typemap<panda::unievent::http::client::ClientConnectionPool*, TYPE> : TypemapObject<panda::unievent::http::client::ClientConnectionPool*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMGBackref> {
        static std::string package () { return "UniEvent::HTTP::ClientConnectionPool"; }
    };
} // namespace xs
