#pragma once
#include "msg.h"
#include "Error.h"
#include "Response.h"
#include <panda/unievent/Timer.h>
#include <panda/CallbackDispatcher.h>

namespace panda { namespace unievent { namespace http {

struct Client;  using ClientSP  = iptr<Client>;
struct Request; using RequestSP = iptr<Request>;

struct NetLoc {
    string   host;
    uint16_t port;

    bool operator== (const NetLoc& other) const { return host == other.host && port == other.port; }
};
std::ostream& operator<< (std::ostream& os, const NetLoc& h);

struct Request : protocol::http::Request {
    struct Builder;
    using response_fptr = void(const RequestSP&, const ResponseSP&, const std::error_code&);
    using partial_fptr  = void(const RequestSP&, const ResponseSP&, const std::error_code&);
    using redirect_fptr = void(const RequestSP&, const ResponseSP&, const URISP&);
    using continue_fptr = void(const RequestSP&);
    using response_fn   = function<response_fptr>;
    using partial_fn    = function<partial_fptr>;
    using redirect_fn   = function<redirect_fptr>;
    using continue_fn   = function<continue_fptr>;

    static constexpr const uint64_t DEFAULT_TIMEOUT           = 20000; // [ms]
    static constexpr const uint16_t DEFAULT_REDIRECTION_LIMIT = 20;    // [hops]

    uint64_t                          timeout           = DEFAULT_TIMEOUT;
    bool                              follow_redirect   = true;
    uint16_t                          redirection_limit = DEFAULT_REDIRECTION_LIMIT;
    CallbackDispatcher<response_fptr> response_event;
    CallbackDispatcher<partial_fptr>  partial_event;
    CallbackDispatcher<redirect_fptr> redirect_event;
    CallbackDispatcher<continue_fptr> continue_event;

    Request () {}

    NetLoc       netloc       () const { return { uri->host(), uri->port() }; }
    const URISP& original_uri () const { return _original_uri; }

    bool transfer_completed () const { return _transfer_completed; }

    void send_chunk        (const string& chunk);
    void send_final_chunk  ();

    void cancel (const std::error_code& = make_error_code(std::errc::operation_canceled));

protected:
    protocol::http::ResponseSP new_response () const override { return new Response(); }

private:
    friend Client;

    URISP    _original_uri;
    uint16_t _redirection_counter = 0;
    bool     _transfer_completed  = false;
    ClientSP _client; // holds client when active
    TimerSP  _timer;
};

struct Request::Builder : protocol::http::Request::BuilderImpl<Builder> {
    Builder& response_callback (const Request::response_fn& cb) {
        _response_callback = cb;
        return *this;
    }

    Builder& partial_callback (const Request::partial_fn& cb) {
        _partial_callback = cb;
        return *this;
    }

    Builder& redirect_callback (const Request::redirect_fn& cb) {
        _redirect_callback = cb;
        return *this;
    }

    Builder& timeout (uint64_t timeout) {
        _timeout = timeout;
        return *this;
    }

    Builder& follow_redirect (bool val) {
        _follow_redirect = val;
        return *this;
    }

    Builder& redirection_limit (uint16_t redirection_limit) {
        _redirection_limit = redirection_limit;
        return *this;
    }

    RequestSP build () {
        RequestSP r = new Request();
        r->method            = _method;
        r->uri               = _uri;
        r->headers           = std::move(_headers);
        r->body              = std::move(_body);
        r->chunked           = _chunked;
        r->http_version      = _http_version;
        r->timeout           = _timeout;
        r->follow_redirect   = _follow_redirect;
        r->redirection_limit = _redirection_limit;

        if (_response_callback) r->response_event.add(_response_callback);
        if (_partial_callback)  r->partial_event.add(_partial_callback);
        if (_redirect_callback) r->redirect_event.add(_redirect_callback);

        return r;
    }

protected:
    Request::response_fn _response_callback;
    Request::partial_fn  _partial_callback;
    Request::redirect_fn _redirect_callback;
    uint64_t             _timeout           = Request::DEFAULT_TIMEOUT;
    bool                 _follow_redirect   = true;
    uint16_t             _redirection_limit = Request::DEFAULT_REDIRECTION_LIMIT;
};

}}}
