#pragma once

#include <panda/CallbackDispatcher.h>
#include <panda/function.h>
#include <panda/function_utils.h>
#include <panda/protocol/http/Response.h>
#include <panda/string.h>
#include <panda/unievent/Debug.h>

#include "../common/Defines.h"

namespace panda { namespace unievent { namespace http {

class Response : public protocol::http::Response {
public:
    Response() { _ECTOR(); }

    Response(int                                       code,
             const string&                             reason,
             protocol::http::Header&&                  header,
             protocol::http::BodySP                    body,
             const string&                             http_version,
             bool                                      chunked,
             function<void(ResponseSP, const string&)> error_cb)
            : protocol::http::Response(code, reason, std::move(header), body, http_version), chunked_(chunked) {
        _ECTOR();
        error_callback.add(error_cb);
    }

    struct Builder {
        Builder& header(protocol::http::Header&& header) {
            header_ = std::move(header);
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

            if (!body_) {
                if (chunked_) {
                    header_.add_field("Transfer-Encoding", "chunked");
                    header_.add_field("Content-Type", content_type_);
                } else {
                    body_ = make_iptr<protocol::http::Body>();
                }
            } else {
                header_.add_field("Content-Length", to_string(body_->content_length()));
                header_.add_field("Content-Type", content_type_);
            }

            return new Response(code_, reason_, std::move(header_), body_, http_version_, chunked_, error_callback_);
        }

    protected:
        protocol::http::Header                    header_;
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

    CallbackDispatcher<void(ResponseSP, const string&)> error_callback;
    CallbackDispatcher<void(const string&, bool)>       write_callback;

    std::string dump() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

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

}}} // namespace panda::unievent::http