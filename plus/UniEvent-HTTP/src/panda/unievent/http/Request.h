#pragma once
#include "msg.h"
#include "Error.h"
#include "Response.h"
#include <panda/unievent/Timer.h>
#include <panda/unievent/Stream.h> // SSL_CTX
#include <panda/CallbackDispatcher.h>

namespace panda { namespace unievent { namespace http {

struct Client;  using ClientSP  = iptr<Client>;
struct Request; using RequestSP = iptr<Request>;

struct NetLoc {
    string   host;
    uint16_t port = 0;

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
    SSL_CTX*                          ssl_ctx           = nullptr;
    URISP                             proxy;

    Request () {}

    const URISP& original_uri () const { return _original_uri; }

    bool transfer_completed () const { return _transfer_completed; }

    void send_chunk        (const string& chunk);
    void send_final_chunk  ();

    void cancel (const std::error_code& = make_error_code(std::errc::operation_canceled));

protected:
    protocol::http::ResponseSP new_response () const override { return new Response(); }

private:
    friend Client; friend struct Pool;

    URISP    _original_uri;
    uint16_t _redirection_counter = 0;
    bool     _transfer_completed  = false;
    ClientSP _client; // holds client when active
    TimerSP  _timer;

    NetLoc netloc () const { return { uri->host(), uri->port() }; }

    void check () const {
        if (!uri) throw HttpError("request must have uri");
        if (!uri->host()) throw HttpError("uri must have host");
    }
};

struct Request::Builder : protocol::http::Request::BuilderImpl<Builder, RequestSP> {
    Builder () : BuilderImpl(new Request()) {}

    Builder& response_callback (const Request::response_fn& cb) {
        _message->response_event.add(cb);
        return *this;
    }

    Builder& partial_callback (const Request::partial_fn& cb) {
        _message->partial_event.add(cb);
        return *this;
    }

    Builder& redirect_callback (const Request::redirect_fn& cb) {
        _message->redirect_event.add(cb);
        return *this;
    }

    Builder& timeout (uint64_t timeout) {
        _message->timeout = timeout;
        return *this;
    }

    Builder& follow_redirect (bool val) {
        _message->follow_redirect = val;
        return *this;
    }

    Builder& redirection_limit (uint16_t redirection_limit) {
        _message->redirection_limit = redirection_limit;
        return *this;
    }

    Builder& ssl_ctx (SSL_CTX* ctx) {
        _message->ssl_ctx = ctx;
        return *this;
    }

    Builder& proxy (const URISP& proxy) {
        _message->proxy = proxy;
        return *this;
    }
};

}}}
