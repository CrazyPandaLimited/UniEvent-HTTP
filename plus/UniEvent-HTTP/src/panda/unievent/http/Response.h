#pragma once
#include <panda/CallbackDispatcher.h>
#include <panda/protocol/http/Response.h>

namespace panda { namespace unievent { namespace http {

struct Response;
using ResponseSP = iptr<Response>;

using protocol::http::Body;
using protocol::http::Header;

struct Response : protocol::http::Response {
    struct Builder;
    using error_fptr = void(const ResponseSP&, const string&);
    using error_fn   = function<error_fptr>;
    using write_fptr = void(const string& chunk, bool is_last);

    CallbackDispatcher<error_fptr> error_event;
    CallbackDispatcher<write_fptr> write_event;

    Response () {}

    Response (int             code,
              const string&   reason,
              Header&&        header,
              Body&&          body,
              const string&   http_version,
              bool            chunked,
              const error_fn& error_cb)
        : protocol::http::Response(code, reason, std::move(header), std::move(body), http_version, chunked)
    {
        if (error_cb) error_event.add(error_cb);
    }

    void write_chunk (const string& chunk, bool is_last = false) {
        write_event(chunk, is_last);
    }

protected:
    // restrict stack allocation
    ~Response () {}

private:
    // disable copying as we disabled stack usage
    Response (const Response&) = delete;
    Response& operator= (const Response&) = delete;
};

struct Response::Builder : protocol::http::Response::BuilderImpl<Builder> {
    Builder& error_callback (const Response::error_fn& cb) {
        _error_callback = cb;
        return *this;
    }

    ResponseSP build () {
        return new Response(_code, _message, std::move(_headers), std::move(_body), _http_version, _chunked, _error_callback);
    }

protected:
    Response::error_fn _error_callback;
};

}}}
