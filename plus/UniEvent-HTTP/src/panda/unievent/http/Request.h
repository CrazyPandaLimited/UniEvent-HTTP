#pragma once
#include "msg.h"
#include "Error.h"
#include "Response.h"
#include <panda/unievent/Timer.h>
#include <panda/CallbackDispatcher.h>

namespace panda { namespace unievent { namespace http {

struct Client;
struct Request;
using RequestSP = iptr<Request>;

struct NetLoc {
    string   host;
    uint16_t port;

    bool operator== (const NetLoc& other) const { return host == other.host && port == other.port; }
};
std::ostream& operator<< (std::ostream& os, const NetLoc& h);

struct Request : protocol::http::Request {
    struct Builder;
    using response_fptr = void(const RequestSP&, const ResponseSP&, const std::error_code&);
    using partial_fptr  = void(const RequestSP&, const ResponseSP&, State state, const std::error_code&);
    using redirect_fptr = void(const RequestSP&, const ResponseSP&, const URISP&);
    using response_fn   = function<response_fptr>;
    using partial_fn    = function<partial_fptr>;
    using redirect_fn   = function<redirect_fptr>;

    static constexpr const uint64_t DEFAULT_TIMEOUT           = 20000; // [ms]
    static constexpr const uint16_t DEFAULT_REDIRECTION_LIMIT = 20;    // [hops]

    string                            host;
    uint16_t                          port;
    uint64_t                          timeout;
    uint16_t                          redirection_limit;
    CallbackDispatcher<response_fptr> response_event;
    CallbackDispatcher<partial_fptr>  partial_event;
    CallbackDispatcher<redirect_fptr> redirect_event;

    Request () : port(), timeout(DEFAULT_TIMEOUT), redirection_limit(DEFAULT_REDIRECTION_LIMIT), _client() {}

    Request (Method method, const URISP& uri, Header&& header, Body&& body, int http_version, bool chunked,
             const response_fn& response_cb, const partial_fn& partial_cb, const redirect_fn& redirect_cb,
             uint64_t timeout, uint16_t redirection_limit, const string& host, uint16_t port) :
        protocol::http::Request(method, uri, std::move(header), std::move(body), chunked, http_version),
        host(host), port(port), timeout(timeout), redirection_limit(redirection_limit), _original_uri(uri), _redirection_counter(), _client()
    {
        if (response_cb) response_event.add(response_cb);
        if (partial_cb)  partial_event.add(partial_cb);
        if (redirect_cb) redirect_event.add(redirect_cb);
    }

    NetLoc       netloc       () const { return { host ? host : uri->host(), port ? port : uri->port() }; }
    const URISP& original_uri () const { return _original_uri; }

    void send_chunk (const string& chunk);
    void end_chunk  ();

    void cancel (const std::error_code& = make_error_code(std::errc::operation_canceled));

protected:
    protocol::http::ResponseSP create_response () const override { return new Response(); }

private:
    friend Client;

    URISP    _original_uri;
    uint16_t _redirection_counter;
    Client*  _client;
    TimerSP  _timer;

    void handle_partial (const RequestSP& req, const ResponseSP& res, State state, const std::error_code& err) {
        partial_event(req, res, state, err);
    }

    void handle_response (const RequestSP& req, const ResponseSP& res, const std::error_code& err) {
        response_event(req, res, err);
    }

    void handle_redirect (const RequestSP& req, const ResponseSP& res, const URISP& to) {
        redirect_event(req, res, to);
    }
};

struct Request::Builder : protocol::http::Request::BuilderImpl<Builder> {
    Builder& host (const string& host) {
        _host = host;
        return *this;
    }

    Builder& port (uint16_t port) {
        _port = port;
        return *this;
    }

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

    Builder& redirection_limit (uint16_t redirection_limit) {
        _redirection_limit = redirection_limit;
        return *this;
    }

    RequestSP build () {
        return new Request(
            _method, _uri, std::move(_headers), std::move(_body), _http_version, _chunked,
            _response_callback, _partial_callback, _redirect_callback, _timeout, _redirection_limit, _host, _port
        );
    }

protected:
    string               _host;
    uint16_t             _port = 0;
    Request::response_fn _response_callback;
    Request::partial_fn  _partial_callback;
    Request::redirect_fn _redirect_callback;
    uint64_t             _timeout = Request::DEFAULT_TIMEOUT;
    uint16_t             _redirection_limit = Request::DEFAULT_REDIRECTION_LIMIT;
};

}}}
