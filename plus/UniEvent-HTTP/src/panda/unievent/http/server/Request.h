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
#include "Response.h"

namespace panda { namespace unievent { namespace http { namespace server {

// Host field builder
inline string to_host(iptr<uri::URI> uri) {
    if(uri->port() == 80) {
        return uri->host();
    } else {
        return uri->host() + ":" + to_string(uri->port());
    }
}

class Request : public protocol::http::Request {
public:
    Request(protocol::http::Request::Method method, 
        URISP uri, 
        protocol::http::HeaderSP header, 
        protocol::http::BodySP body, 
        const string& http_version,
        function<void(RequestSP, ResponseSP)> response_c,
        function<void(RequestSP, const string&)> error_c,
        iptr<event::Loop> loop,
        uint64_t timeout) :
            timeout_(timeout),
            connection_(nullptr),
            original_uri_(uri),
            close_timer_(make_iptr<event::Timer>(loop)),
            loop_(loop),
            connection_pool_(nullptr),
            request_counter_(0),
            part_counter_(0),
            protocol::http::Request(method, uri, header, body, http_version) {
        panda_log_debug("ctor");    

        response_callback.add(response_c);
        error_callback.add(error_c);

        close_timer_->timer_event.add([&](event::Timer* timer) {
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
        
        Builder& error_callback(function<void(RequestSP, const string&)> cb) {
            error_callback_ = cb;
            return *this;
        }
        
        Builder& loop(iptr<event::Loop> loop) {
            loop_ = loop;
            return *this;
        }
        
        Builder& timeout(uint64_t timeout) {
            timeout_ = timeout;
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

            if(!loop_) {
                loop_ = event::Loop::default_loop();
                panda_log_debug("request, default loop");
            }

            if(!timeout_) {
                timeout_ = 4000;
            }
            
            return new Request(method_, uri_, header_, body_, http_version_, response_callback_, error_callback_, loop_, timeout_);
        }

        protected:
            protocol::http::HeaderSP header_;
            protocol::http::BodySP body_;
            string http_version_;
            protocol::http::Request::Method method_;
            URISP uri_;
            string content_type_;
            function<void(RequestSP, ResponseSP)> response_callback_;
            function<void(RequestSP, const string&)> error_callback_;
            iptr<event::Loop> loop_;
            uint64_t timeout_ = {};
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

    virtual void on_connect(int dummy) {
        panda_log_debug("on_connect");
        
        redirection_counter_ = 0;
        close_timer_->again();
    }

    virtual void on_response(ResponseSP response_ptr) {
        panda_log_debug("on_response");
        response_callback(this, response_ptr);
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
    
    void reset_close_timer() {
        panda_log_debug("reset_close_timer");
        close_timer_->again();
    }

private:
    // disable copying as we disabled stack usage
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

private:
    iptr<protocol::http::ResponseParser> response_parser_;  
    uint8_t redirection_counter_;
    uint64_t request_counter_;
    uint64_t part_counter_;
    iptr<event::Loop> loop_;
    ClientConnectionPool* connection_pool_;
    client::Connection* connection_;
    uint64_t timeout_;
    uint8_t redirection_limit_;
    URISP original_uri_;
    iptr<event::Timer> close_timer_;
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
    header_str += string(to_string(request_ptr->method())) + " " + request_ptr->uri()->to_string() + " " + "HTTP/" + request_ptr->http_version() + "\r\n";
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

}}}} // namespace panda::unievent::http::server
