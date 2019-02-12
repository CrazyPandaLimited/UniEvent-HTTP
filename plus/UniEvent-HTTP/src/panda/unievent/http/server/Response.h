#pragma once

#include <panda/CallbackDispatcher.h>
#include <panda/function.h>
#include <panda/function_utils.h>
#include <panda/protocol/http/Response.h>
#include <panda/string.h>

#include "../common/Defines.h"

namespace panda { namespace unievent { namespace http { namespace server {

class Response : public protocol::http::Response {
    friend Connection;

public:
    Response() { _ECTOR(); }

    Response(int                                       code,
             const string&                             reason,
             protocol::http::HeaderSP                  header,
             protocol::http::BodySP                    body,
             const string&                             http_version,
             bool                                      chunked,
             function<void(ResponseSP, const string&)> error_cb)
            : protocol::http::Response(code, reason, header, body, http_version), chunked_(chunked) {
        _ECTOR();
        error_callback.add(error_cb);
    }

    struct Builder {
        Builder& header(protocol::http::HeaderSP header) {
            header_ = header;
            return *this;
        }

        Builder& body(protocol::http::BodySP body) {
            body_ = body;
            return *this;
        }

        Builder& body(const string& body, const string& content_type = "text/plain") {
            body_         = make_iptr<protocol::http::Body>(body);
            content_type_ = content_type;
            return *this;
        }

        Builder& version(const string& http_version) {
            http_version_ = http_version;
            return *this;
        }

        Builder& code(int code) {
            code_ = code;
            return *this;
        }

        Builder& reason(const string& reason) {
            reason_ = reason;
            return *this;
        }

        Builder& chunked(const string& content_type = "text/plain") {
            chunked_      = true;
            content_type_ = content_type;
            return *this;
        }

        Builder& error_callback(function<void(ResponseSP, const string&)> cb) {
            error_callback_ = cb;
            return *this;
        }

        Response* build() {
            if (http_version_.empty()) {
                http_version_ = "1.1";
            }

            if (!header_) {
                header_ = make_iptr<protocol::http::Header>();
            }

            if (!body_) {
                if (chunked_) {
                    header_->add_field("Transfer-Encoding", "chunked");
                    header_->add_field("Content-Type", content_type_);
                } else {
                    body_ = make_iptr<protocol::http::Body>();
                }
            } else {
                header_->add_field("Content-Length", to_string(body_->content_length()));
                header_->add_field("Content-Type", content_type_);
            }

            return new Response(code_, reason_, header_, body_, http_version_, chunked_, error_callback_);
        }

    protected:
        protocol::http::HeaderSP                  header_;
        protocol::http::BodySP                    body_;
        string                                    http_version_;
        int                                       code_ = {};
        string                                    reason_;
        string                                    content_type_;
        bool                                      chunked_ = {};
        function<void(ResponseSP, const string&)> error_callback_;
    };

    bool chunked() const { return chunked_; }

    void write_chunk(const string& chunk, bool is_last = false) {
        _EDEBUGTHIS("write_chunk, is_last: %d", is_last);
        write_callback(chunk, is_last);
    }

    CallbackDispatcher<void(const string&, bool)>       write_callback;
    CallbackDispatcher<void(ResponseSP, const string&)> error_callback;

protected:
    // restrict stack allocation
    virtual ~Response() { _EDTOR(); }

private:
    // disable copying as we disabled stack usage
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;

private:
    bool chunked_;
};

inline std::ostream& operator<<(std::ostream& os, const ResponseSP& ptr) {
    if (ptr) {
        os << *ptr;
    }
    return os;
}

inline std::vector<string> to_vector(ResponseSP response_ptr) {
    string header_str;
    header_str += string("HTTP/") + response_ptr->http_version() + " " + to_string(response_ptr->code()) + " " + response_ptr->reason() + "\r\n";
    for (auto field : response_ptr->header()->fields) {
        header_str += field.name + ": " + field.value + "\r\n";
    }

    header_str += "\r\n";

    std::vector<string> result;
    if (response_ptr->chunked()) {
        result.emplace_back(header_str);
    } else {
        result.reserve(1 + response_ptr->body()->parts.size());
        result.emplace_back(header_str);
        for (auto part : response_ptr->body()->parts) {
            result.emplace_back(part);
        }
    }

    return result;
}

}}}} // namespace panda::unievent::http::server
