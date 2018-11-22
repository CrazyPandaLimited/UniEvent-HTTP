#pragma once

#include <cstdint>

#include <panda/log.h>
#include <panda/refcnt.h>
#include <panda/string.h>
#include <panda/uri/URI.h>
#include <panda/unievent/Timer.h>
#include <panda/CallbackDispatcher.h>

#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/protocol/http/ResponseParser.h>

#include "../common/Defines.h"

#include "Connection.h"
#include "ConnectionPool.h"

#include "Response.h"

namespace panda { namespace unievent { namespace http { namespace client {

// Host field builder
// See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Host
inline string to_host(iptr<uri::URI> uri) {
    if(uri->port() == 80 || uri->port() == 443) {
        return uri->host();
    } else {
        return uri->host() + ":" + to_string(uri->port());
    }
}

inline ClientConnectionPool* get_thread_local_connection_pool() {
    thread_local ClientConnectionPool pool;
    thread_local ClientConnectionPool* pool_ptr = &pool;
    return pool_ptr;
}

using URISP = iptr<uri::URI>;

class Request : public protocol::http::Request {
    friend client::Connection;
    friend void http_request(RequestSP request, iptr<client::Connection> connection);
    friend iptr<client::Connection> http_request(RequestSP request, ClientConnectionPool* connection_pool);

public:
    Request(protocol::http::Request::Method method, 
        URISP uri, 
        protocol::http::HeaderSP header, 
        protocol::http::BodySP body, 
        const string& http_version,
        function<void(RequestSP, ResponseSP)> response_c,
        function<void(RequestSP, URISP)> redirect_c,
        function<void(RequestSP, const string&)> error_c,
        iptr<Loop> loop,
        uint64_t timeout,
        uint8_t redirection_limit) :
            protocol::http::Request(method, uri, header, body, http_version),
            loop_(loop),
            close_timer_(new Timer(loop)),
            original_uri_(uri),
            timeout_(timeout),
            redirection_limit_(redirection_limit),
            request_counter_(0),
            part_counter_(0),
            connection_(nullptr),
            connection_pool_(nullptr) {
        panda_log_debug("ctor");    

        response_callback.add(response_c);
        redirect_callback.add(redirect_c);
        error_callback.add(error_c);

        close_timer_->timer_event.add([&](Timer*) {
            panda_log_debug("ticking close timer");
            on_any_error("timeout exceeded: " + to_string(timeout_));
        });
    }
 
    struct Builder {
    public:
        Builder& header(protocol::http::HeaderSP header) {
            header_ = header;
            return *this;
        }

        Builder& body(protocol::http::BodySP body) {
            body_ = body;
            return *this;
        }

        Builder& body(const string& body, const string& content_type = "text/plain") {
            body_ = make_iptr<protocol::http::Body>(body);
            content_type_ = content_type;
            return *this;
        }

        Builder& version(const string& http_version) {
            http_version_ = http_version;
            return *this;
        }

        Builder& method(protocol::http::Request::Method method) {
            method_ = method;
            return *this;
        }

        Builder& uri(const string& uri) {
            uri_ = make_iptr<uri::URI>(uri);
            return *this;
        }

        Builder& uri(URISP uri) {
            uri_ = uri;
            return *this;
        }

        Builder& response_callback(function<void(RequestSP, ResponseSP)> cb) {
            response_callback_ = cb;
            return *this;
        }
        
        Builder& redirect_callback(function<void(RequestSP, URISP)> cb) {
            redirect_callback_ = cb;
            return *this;
        }
        
        Builder& error_callback(function<void(RequestSP, const string&)> cb) {
            error_callback_ = cb;
            return *this;
        }
        
        Builder& loop(iptr<Loop> loop) {
            loop_ = loop;
            return *this;
        }
        
        Builder& timeout(uint64_t timeout) {
            timeout_ = timeout;
            return *this;
        }
        
        Builder& redirection_limit(uint8_t redirection_limit) {
            redirection_limit_ = redirection_limit;
            return *this;
        }

        Request* build() {
            if(http_version_.empty()) {
                http_version_ = "1.1";
            }

            if(!header_) {
                header_ = make_iptr<protocol::http::Header>();
            }

            if(http_version_ == "1.1") {
                if(!header_->has_field("Host")) {
                    header_->add_field("Host", to_host(uri_));
                }
            }

            if(!body_) {
                body_ = make_iptr<protocol::http::Body>();
            } else {
                header_->add_field("Content-Length", to_string(body_->content_length()));
                header_->add_field("Content-Type", content_type_);
            }

            if(!header_->has_field("User-Agent")) {
                header_->add_field("User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.111 Safari/537.36 Panda/0.1.1");
            }

            if(!loop_) {
                loop_ = Loop::default_loop();
                panda_log_debug("request, default loop");
            }

            if(!timeout_) {
                timeout_ = 4000;
            }
            
            if(!redirection_limit_) {
                redirection_limit_ = 20;
            }

            return new Request(method_, uri_, header_, body_, http_version_, response_callback_, redirect_callback_, error_callback_, loop_, timeout_, redirection_limit_);
        }

        protected:
            protocol::http::HeaderSP header_;
            protocol::http::BodySP body_;
            string http_version_;
            protocol::http::Request::Method method_;
            URISP uri_;
            string content_type_;
            function<void(RequestSP, ResponseSP)> response_callback_;
            function<void(RequestSP, URISP)> redirect_callback_;
            function<void(RequestSP, const string&)> error_callback_;
            iptr<Loop> loop_;
            uint64_t timeout_ = {};
            uint8_t redirection_limit_ = {};
    };

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
        panda_log_debug("on_start");

        request_counter_++;
        part_counter_ = 0;
        
        panda_log_debug("request to: " << uri_->to_string() << " " << request_counter_);

        close_timer_->start(timeout_);
    }

protected:
    // restrict stack allocation
    virtual ~Request() {
        panda_log_debug("dtor");
    }
    
    virtual protocol::http::ResponseSP create_response() const override {
        return response();
    }

    virtual void on_connect(int) {
        panda_log_debug("on_connect");
        redirection_counter_ = 0;
        close_timer_->again();
    }

    virtual void on_response(ResponseSP response_ptr) {
        panda_log_debug("on_response");
        redirection_counter_ = 0;
        response_callback(this, response_ptr);
    }
    
    virtual void on_redirect(URISP uri) {
        panda_log_debug("on_redirect");
        if(redirection_counter_++ >= redirection_limit_) {
            on_any_error("redirection limit exceeded");
            return;
        }

        header()->set_field("Host", to_host(uri));

        uri_ = uri;
        
        panda_log_debug("on_redirect: "<< this);

        redirect_callback(this, uri_);
        
        detach_connection();

        if(connection_pool_) {
            connection_ = connection_pool_->get(uri_->host(), uri_->port());
            panda_log_debug("on_redirect, got connection: " << connection_);
            connection_->request(this);
        }
    }

    virtual void on_stop() {
        panda_log_warn("on_stop");
        detach_connection();
    }
    
    virtual void on_any_error(const string& error_str) {
        panda_log_warn("on_any_error: " << error_str);
        error_callback(this, error_str);
        detach_connection();
    }

    void detach_connection() {
        panda_log_debug("detach_connection");

        close_timer_->stop();

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
        panda_log_debug("reset_timer");
        close_timer_->again();
    }
    
    void stop_timer() {
        panda_log_debug("stop_timer");
        close_timer_->stop();
    }

private:
    // disable copying as we disabled stack usage
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

private:
    iptr<Loop> loop_;
    iptr<Timer> close_timer_;
    URISP original_uri_;
    uint64_t timeout_;
    uint8_t redirection_limit_;
    uint8_t redirection_counter_;
    uint64_t request_counter_;
    uint64_t part_counter_;
    client::Connection* connection_;
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
    for(auto field : request_ptr->header()->fields) {
        header_str += field.name + ": " + field.value + "\r\n";
    }

    header_str += "\r\n";

    result.emplace_back(header_str);
    for(auto part : request_ptr->body()->parts) {
        result.emplace_back(part);
    }

    return result;
}

inline void http_request(RequestSP request, iptr<client::Connection> connection) {
    panda_log_debug("http_request, connection: " << &connection);
    if(request->connection_) {
        throw ProgrammingError("Can't reuse incompleted request"); 
    }
    request->connection_ = connection;
    connection->request(request);
}

inline iptr<client::Connection> http_request(RequestSP request, ClientConnectionPool* connection_pool = get_thread_local_connection_pool()) {
    panda_log_debug("http_request, pooled, pool: " << connection_pool);

    if(connection_pool->loop().get() != request->loop_.get()) {
        throw ProgrammingError("Loop mismatch, connection pool loop != loop"); 
    }
    request->connection_pool_ = connection_pool;
    iptr<client::Connection> connection = connection_pool->get(request->uri()->host(), request->uri()->port());
    http_request(request, connection);
    return connection;
}

}}}} // namespace panda::unievent::http::client
