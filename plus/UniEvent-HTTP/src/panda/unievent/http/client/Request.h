#pragma once

#include <cstdint>
#include <cinttypes>

#include <panda/refcnt.h>
#include <panda/string.h>
#include <panda/unievent/Timer.h>
#include <panda/CallbackDispatcher.h>

#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/protocol/http/ResponseParser.h>

#include "../common/Defines.h"

#include "Connection.h"
#include "ConnectionPool.h"

#include "Response.h"

namespace panda { namespace unievent { namespace http {
namespace client {

// Host field builder
// See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Host
inline string to_host(URISP uri) {
    if(uri->port() == 80 || uri->port() == 443) {
        return uri->host();
    } else {
        return uri->host() + ":" + to_string(uri->port());
    }
}

class Request : public protocol::http::Request {
    friend Connection;
    friend void http::http_request(client::RequestSP, client::ConnectionSP);
    friend ConnectionSP http::http_request(client::RequestSP, client::ClientConnectionPool*);

public:
    static constexpr uint64_t DEFAULT_CONNECT_TIMEOUT   = 4000; // [ms]
    static constexpr uint64_t DEFAULT_REDIRECTION_LIMIT = 20;   // [hops]

    Request(protocol::http::Request::Method method,
        URISP uri,
        protocol::http::Header&& header,
        protocol::http::BodySP body,
        const string& http_version,
        ResponseCallback response_c,
        RedirectCallback redirect_c,
        ErrorCallback error_c,
        uint64_t timeout,
        uint8_t redirection_limit) :
            original_uri_(uri),
            timeout_(timeout ? timeout : DEFAULT_CONNECT_TIMEOUT),
            redirection_limit_(redirection_limit ? redirection_limit : DEFAULT_REDIRECTION_LIMIT),
            request_counter_(0),
            part_counter_(0),
            connection_(nullptr),
            connection_pool_(nullptr) {
        _ECTOR();

        init_defaults(method, uri, std::move(header), body, http_version);

        response_callback.add(response_c);
        redirect_callback.add(redirect_c);
        error_callback.add(error_c);
    }

    void init_defaults(Method method,
            URISP uri,
            protocol::http::Header&& header,
            const protocol::http::BodySP& body,
            const string& http_version) {
        method_ = method;
        uri_ = uri;
        http_version_ = http_version ? http_version : "1.1";
        header_ = std::move(header);

        if (http_version_ == "1.1") {
            if (!header_.has_field("Host")) {
                header_.add_field("Host", to_host(uri_));
            }
        }

        if (body) {
            body_ = body;
            header_.add_field("Content-Length", to_string(body_->content_length()));
            if (!header_.has_field("Content-Type")) {
                header_.add_field("Content-Type", "text/plain");
            }
        } else {
            body_ = make_iptr<protocol::http::Body>();
        }

        if (!header_.has_field("User-Agent")) {
            header_.add_field(
                "User-Agent",
                "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.111 Safari/537.36 Panda/1.0.1");
        }
    }

    template <class Derived>
    struct BasicBuilder {
    public:
        Derived& concrete () { return static_cast<Derived&>(*this); }

        Derived& header(protocol::http::Header&& header) {
            header_ = std::move(header);
            return concrete();
        }

        Derived& body(protocol::http::BodySP body) {
            body_ = body;
            return concrete();
        }

        Derived& body(const string& body) {
            body_ = make_iptr<protocol::http::Body>(body);
            return concrete();
        }

        Derived& version(const string& http_version) {
            http_version_ = http_version;
            return concrete();
        }

        Derived& method(protocol::http::Request::Method method) {
            method_ = method;
            return concrete();
        }

        Derived& uri(const string& uri) {
            uri_ = make_iptr<uri::URI>(uri);
            return concrete();
        }

        Derived& uri(URISP uri) {
            uri_ = uri;
            return concrete();
        }

        Derived& response_callback(ResponseCallback cb) {
            response_callback_ = cb;
            return concrete();
        }

        Derived& redirect_callback(RedirectCallback cb) {
            redirect_callback_ = cb;
            return concrete();
        }

        Derived& error_callback(ErrorCallback cb) {
            error_callback_ = cb;
            return concrete();
        }

        Derived& timeout(uint64_t timeout) {
            timeout_ = timeout;
            return concrete();
        }

        Derived& redirection_limit(uint8_t redirection_limit) {
            redirection_limit_ = redirection_limit;
            return concrete();
        }

        Request* build() {
            return new Request(method_, uri_, std::move(header_), body_, http_version_, response_callback_, redirect_callback_, error_callback_, timeout_, redirection_limit_);
        }

        protected:
            protocol::http::Header header_;
            protocol::http::BodySP body_;
            string http_version_;
            protocol::http::Request::Method method_ = {Method::GET};
            URISP uri_;
            ResponseCallback response_callback_;
            RedirectCallback redirect_callback_;
            ErrorCallback error_callback_;
            uint64_t timeout_ = {};
            uint8_t redirection_limit_ = {};
    };

    struct Builder : BasicBuilder<Builder> {};

    ResponseSP response() const {
        return make_iptr<Response>();
    }

    uint64_t timeout() const {
        return timeout_;
    }

    CallbackDispatcher<void(RequestSP, const string&)> error_callback;
    CallbackDispatcher<void(RequestSP)> detach_connection_callback;
    CallbackDispatcher<void(RequestSP, ResponseSP)> response_callback;
    CallbackDispatcher<void(RequestSP, URISP)> redirect_callback;

    virtual void on_start() {
        _EDEBUGTHIS("on_start: counter: %" PRIu64 "", request_counter_);

        request_counter_++;
        part_counter_ = 0;

        reset_timer();
    }

    std::string dump() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

protected:
    // restrict stack allocation
    virtual ~Request() {
        _EDTOR();
    }

    virtual protocol::http::ResponseSP create_response() const override {
        return response();
    }

    virtual void on_connect(int) {
        _EDEBUGTHIS();
        redirection_counter_ = 0;
        reset_timer();
    }

    virtual void on_response(ResponseSP response_ptr) {
        _EDEBUGTHIS();
        redirection_counter_ = 0;
        response_callback(this, response_ptr);
    }

    virtual void on_redirect(URISP uri) {
        _EDEBUGTHIS();
        if(redirection_counter_++ >= redirection_limit_) {
            on_any_error("redirection limit exceeded");
            return;
        }

        header().set_field("Host", to_host(uri));

        uri_ = uri;

        redirect_callback(this, uri_);

        detach_connection();

        if(connection_pool_) {
            connection_ = connection_pool_->get(uri_->host(), uri_->port());
            _EDEBUGTHIS("on_redirect, connection: %p", connection_);
            connection_->request(this);
        }
    }

    virtual void on_stop() {
        _EDEBUGTHIS();
        detach_connection();
    }

    virtual void on_any_error(const string& error_str) {
        _EDEBUGTHIS();
        error_callback(this, error_str);
        detach_connection();
    }

    void detach_connection() {
        _EDEBUGTHIS();
        stop_timer();
        if(connection_) {
            connection_->detach(this);
            if(connection_pool_) {
                connection_pool_->detach(connection_);
            }
            detach_connection_callback(this);
            connection_ = nullptr;
        }
    }

    void reset_timer() {
        _EDEBUGTHIS();
        if(close_timer_) {
            close_timer_->again();
        }
        else {
            LoopSP loop;
            if(connection_) {
                loop = connection_->loop();
            } else if(connection_pool_) {
                loop = connection_pool_->loop();
            } else {
                throw ProgrammingError("No connection and no pool, don't know where to find loop for a timer");
            }

            _EDEBUGTHIS("using loop: %p", loop.get());

            close_timer_ = Timer::start(timeout_, [this](Timer*) {
                _EDEBUGTHIS("ticking close timer");
                on_any_error("timeout exceeded: " + to_string(timeout_));
            }, loop);
        }
    }

    void stop_timer() {
        _EDEBUGTHIS();
        if(close_timer_) {
            close_timer_->stop();
        }
    }

private:
    // disable copying as we disabled stack usage
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

private:
    TimerSP close_timer_;
    URISP original_uri_;
    uint64_t timeout_;
    uint8_t redirection_limit_;
    uint8_t redirection_counter_;
    uint64_t request_counter_;
    uint64_t part_counter_;
    Connection* connection_;
    ClientConnectionPool* connection_pool_;
};

inline
std::ostream& operator<<(std::ostream& os, const RequestSP& ptr) {
    if(ptr) {
        os << *ptr;
    }
    return os;
}

inline std::vector<string> to_vector(RequestSP request_ptr) {
    std::vector<string> result;
    result.reserve(1 + request_ptr->body()->parts.size());

    string header_str;
    header_str += string(to_string(request_ptr->method())) + " " + request_ptr->uri()->relative() + " " + "HTTP/" + request_ptr->http_version() + "\r\n";
    for(auto field : request_ptr->header().fields) {
        header_str += field.name + ": " + field.value + "\r\n";
    }

    header_str += "\r\n";

    result.emplace_back(header_str);
    for(auto part : request_ptr->body()->parts) {
        result.emplace_back(part);
    }

    return result;
}

} // namespace client

inline void http_request(panda::unievent::http::client::RequestSP request, panda::unievent::http::client::ConnectionSP connection) {
    _EDEBUG("http_request");
    if(request->connection_) {
        throw ProgrammingError("Can't reuse incompleted request");
    }
    request->connection_ = connection;
    connection->request(request);
}

inline panda::unievent::http::client::ClientConnectionPool* get_thread_local_connection_pool() {
    thread_local panda::unievent::http::client::ClientConnectionPool pool;
    thread_local panda::unievent::http::client::ClientConnectionPool* pool_ptr = &pool;
    return pool_ptr;
}

inline client::ConnectionSP http_request(
        client::RequestSP request,
        client::ClientConnectionPool* connection_pool = get_thread_local_connection_pool()) {
    _EDEBUG("http_request, pooled: %p", connection_pool);
    request->connection_pool_ = connection_pool;
    client::ConnectionSP connection = connection_pool->get(request->uri()->host(), request->uri()->port());
    http_request(request, connection);
    return connection;
}

}}} // namespace panda::unievent::http
