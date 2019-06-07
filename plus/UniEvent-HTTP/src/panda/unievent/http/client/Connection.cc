#include "Connection.h"

#include <fstream>
#include <sstream>

#include <panda/refcnt.h>
#include <panda/function.h>

#include <panda/unievent/Debug.h>

#include "../common/Error.h"
#include "../common/HostAndPort.h"

#include "Request.h"
#include "Response.h"
#include "ConnectionPool.h"

namespace panda { namespace unievent { namespace http { namespace client {

Connection::~Connection() {
    _EDTOR();
}

Connection::Connection(const HostAndPort& host_and_port, Loop* loop, ClientConnectionPool* pool, uint64_t inactivity_timeout)
        : TCP(loop)
        , host_and_port_(host_and_port)
        , pool_(pool)
        , inactivity_timeout_(inactivity_timeout)
        , inactivity_timer_(make_iptr<Timer>(loop))
        , response_parser_(make_iptr<protocol::http::ResponseParser>())
        , connected_(false) {
    _ECTOR();
    inactivity_timer_->timer_event.add([=](Timer*) {
        _EDEBUGTHIS("ticking inactivity timer: %p", pool);
        if (pool) {
            pool->erase(this);
        }
    });
}

void Connection::request(RequestSP request) {
    _EDEBUGTHIS("request");

    inactivity_timer_->stop();

    connect_callback.add(function_details::tmp_abstract_function(&Request::on_connect, request));

    request->on_start();

    auto data = to_vector(request);

    if(!connected_) {
        if(host_and_port_.port == 443) {
            use_ssl();
        }
    }

    response_parser_->append_request(request);

    connect(host_and_port_.host, host_and_port_.port);
    read_start();
    write(data.begin(), data.end());
}

void Connection::detach(RequestSP request) {
    _EDEBUGTHIS("detach");
    connect_callback.remove_object(function_details::tmp_abstract_function(&Request::on_connect, request));
    inactivity_timer_->start(inactivity_timeout_);
}

void Connection::connect(const string& host, unsigned short port) {
    _EDEBUGTHIS("connect: %.*s:%d", (int)host.length(), host.data(), port);
    if(connected_) {
        return;
    }
    TCP::connect(host, port);
}

void Connection::close() {
    _EDEBUGTHIS("close");
    connected_ = false;
    shutdown();
    disconnect();
    close_callback(this);
}

void Connection::on_connect(const CodeError* err, ConnectRequest*) {
    _EDEBUGTHIS("on_connect");
    if(err) {
        on_stream_error(err);
        return;
    }

    connected_ = true;
    connect_callback(0);
}

void Connection::on_read(string& _buf, const CodeError* err) {
    _EDEBUGTHIS("on_read: %zu", _buf.size());
    if(err) {
        on_stream_error(err);
        return;
    }

    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING

    for(auto pos = response_parser_->parse(buf); pos->state != protocol::http::ResponseParser::State::not_yet; ++pos) {
        _ETRACETHIS("parser state: %d %zu", static_cast<int>(pos->state), pos->position);
        auto request = static_pointer_cast<Request>(pos->request);
        if(pos->state == protocol::http::ResponseParser::State::failed) {
            _ETRACETHIS("parser failed: %zu", pos->position);
            on_any_error("parser failed");
            return;
        }
        else if(pos->state == protocol::http::ResponseParser::State::got_header) {
            _ETRACETHIS("got header: %zu", pos->position);
        }
        else if(pos->state == protocol::http::ResponseParser::State::got_body) {
            _ETRACETHIS("got body: %zu", pos->position);
        }

        if(pos->response->is_valid()) {
            request->stop_timer();
            on_response(request, static_pointer_cast<Response>(pos->response));
        }

        if(pos->position >= buf.size()) {
            _ETRACETHIS("returning: %zu", pos->position);
            return;
        }
    }
}

void Connection::on_response(RequestSP request, ResponseSP response) {
    _EDEBUGTHIS("on_response: %d", response->code);
    switch(response->code) {
        case 300:
        case 301:
        case 302:
        case 303:
        // disabled for security reasons
        // case 305:
        case 307:
        case 308:
            _ETRACETHIS("on_response, redirect");
            response_parser_->init();
            request->on_redirect(make_iptr<uri::URI>(response->headers.get_field("Location")));
            return;
    }

    request->on_response(response);

    if(response_parser_->no_pending_requests()) {
        request->on_stop();
    }

    return;
}

void Connection::on_stream_error(const CodeError* err) {
    _EDEBUGTHIS("on_stream_error: %s", err->what());
    stream_error_callback(this, err);
    close();
}

void Connection::on_any_error(const string& error_str) {
    _EDEBUGTHIS("on_any_error: %.*s", (int)error_str.size(), error_str.data());
    any_error_callback(this, error_str);
    close();
}

void Connection::on_eof() {
    _EDEBUGTHIS("on_eof");
    close();
    TCP::on_eof();
}

}}}} // namespace panda::unievent::http::client
