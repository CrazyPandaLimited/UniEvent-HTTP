#pragma once
#include "Response.h"
#include "Connection.h"
#include <panda/CallbackDispatcher.h>
#include <panda/protocol/http/Request.h>

namespace panda { namespace unievent { namespace http {

struct Request;
using RequestSP = iptr<Request>;

struct Request : protocol::http::Request {
    struct Builder;
    using Method        = protocol::http::Request::Method;
    using response_fptr = void(const RequestSP&, const ResponseSP&, const std::error_code&);
    using redirect_fptr = void(const RequestSP&, const URISP&);
    using response_fn   = function<response_fptr>;
    using redirect_fn   = function<redirect_fptr>;

    static constexpr const uint64_t DEFAULT_TIMEOUT           = 20000; // [ms]
    static constexpr const uint64_t DEFAULT_REDIRECTION_LIMIT = 20;    // [hops]

    string                            host;
    uint16_t                          port;
    uint64_t                          timeout;
    uint8_t                           redirection_limit;
    CallbackDispatcher<response_fptr> response_event;
    CallbackDispatcher<redirect_fptr> redirect_event;

    Request (Method method, const URISP& uri, Header&& header, Body&& body, const string& http_version, bool chunked,
             const response_fn& response_cb, const redirect_fn& redirect_cb, uint64_t timeout, uint8_t redirection_limit,
             const string& host, uint16_t port) :
        protocol::http::Request(method, uri, std::move(header), std::move(body), http_version, chunked),
        host(host), port(port), timeout(timeout),
        _original_uri(uri),
        _redirection_limit(redirection_limit ? redirection_limit : DEFAULT_REDIRECTION_LIMIT),
        _connection(),
    {
        if (response_cb) response_callback.add(response_cb);
        if (redirect_cb) redirect_callback.add(redirect_cb);
    }

    const URISP& original_uri () const { return _original_uri; }

    uint64_t timeout () const { return _timeout; }

protected:
    ~Request () {} // restrict stack allocation

    ResponseSP create_response () const override { return new Response(); }

    virtual void on_redirect (const URISP& uri) {
        if (_redirection_counter++ >= _redirection_limit) {
            on_any_error("redirection limit exceeded");
            return;
        }

        //headers.set_field("Host", to_host(uri));
        this->uri = uri;

        redirect_event(this, uri);

        detach_connection();

        if (_connection_pool) {
            _connection = _connection_pool->get(uri->host(), uri->port());
            _connection->request(this);
        }
    }

private:
    friend Connection;
    friend void http::http_request(client::RequestSP, client::ConnectionSP);
    friend ConnectionSP http::http_request(client::RequestSP, client::ClientConnectionPool*);

    URISP   _original_uri;
    uint8_t _redirection_counter;
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

    Builder& redirect_callback (const Request::redirect_fn& cb) {
        _redirect_callback = cb;
        return *this;
    }

    Builder& timeout (uint64_t timeout) {
        _timeout = timeout;
        return *this;
    }

    Builder& redirection_limit (uint8_t redirection_limit) {
        _redirection_limit = redirection_limit;
        return *this;
    }

    RequestSP build () {
        return new Request(
            _method, _uri, std::move(_headers), std::move(_body), _http_version, _chunked,
            _response_callback, _redirect_callback, _timeout, _redirection_limit, _host, _port
        );
    }

protected:
    string               _host;
    uint16_t             _port = 0;
    Request::response_fn _response_callback;
    Request::redirect_fn _redirect_callback;
    uint64_t             _timeout = Request::DEFAULT_TIMEOUT;
    uint8_t              _redirection_limit = 0;
};

inline void http_request (const RequestSP& request, const ClientConnectionSP& connection) {
    if (request->_connection) {
        throw ProgrammingError("Can't reuse incompleted request");
    }
    request->_connection = connection;
    connection->request(request);
}

inline ConnectionPool* get_thread_local_connection_pool () {
    static thread_local ConnectionPool* _ptr;
    ConnectionPool* ptr = _ptr;
    if (!ptr) {
        static thread_local ConnectionPoolSP sp = new ConnectionPool();
        ptr = _ptr = panda::get_global_tls_ptr<ConnectionPool>(sp.get(), "instance");
    }
    return ptr;
}

inline ClientConnectionSP http_request (const RequestSP& request, ConnectionPool* pool = get_thread_local_connection_pool()) {
    request->_connection_pool = pool;
    auto connection = pool->get(request->uri->host(), request->uri->port());
    http_request(request, connection);
    return connection;
}

}}}
