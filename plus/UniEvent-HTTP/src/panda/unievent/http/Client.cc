#include "Connection.h"

#include <fstream>
#include <sstream>

#include <panda/refcnt.h>
#include <panda/function.h>

#include <panda/unievent/Debug.h>

#include "../common/Error.h"
#include "../common/HostAndPort.h"

#include "Request.h"
#include "ConnectionPool.h"

namespace panda { namespace unievent { namespace http {

std::ostream& operator<< (std::ostream& os, const NetLoc& h) {
    os << h.host;
    os << ":";
    os << to_string(h.port);
    return os;
}

Client::Client (const LoopSP& loop) : Tcp(loop), _pool(), _netloc({"", 0}), _last_activity_time(0), connected_(false) {
    Tcp::event_listener(this);
}

Client::Client (Pool* pool) : Client(pool->loop()), _pool(pool) {}

Client::~Client () {
    stop();
}

void Client::request (const RequestSP& request) {
    if (_request) throw HttpError("client supports only one request at a time");
    _request = request;

    if (request->timeout) {
        if (!_timer) {
            _timer = new Timer(loop());
            _timer->event_listener(this);
        }
        _timer->once(request->timeout);
    }

    NetLoc netloc = {
        request->host ? request->host : request->uri->host(),
        request->port ? request->port : request->uri->port()
    };

    if (!connected() || _netloc.host !== netloc.host || _netloc.port != netloc.port) {
        if (_netloc.host) { // not first use
            Tcp::clear();
            Tcp::event_listener(this);
        }
        if (request->uri->secure()) use_ssl(); // filters have been cleared in clear()
        _netloc = std::move(netloc);
        connect(_netloc.host, _netloc.port);
    }

    request->_redirection_counter = 0;

    auto data = to_vector(request);
    _response_parser->append_request(request);
    read_start();
    write(data.begin(), data.end());
}

void Client::cancel (const std::error_code& err) {
    if (!_request) return;
    auto req = _request;
    auto res = _response;
    _request.reset();
    _response.reset();
    if (_timer) _timer->stop();
    Tcp::reset();
    if (_pool) _pool->putback(this);
    request->handle_response(req, res, err);
}

void Client::on_connect (const CodeError& err, const ConnectRequestSP&) {
    if (_request && err) cancel(err.code());
}

void Client::on_write (const CodeError& err, const WriteRequestSP&) {
    if (_request && err) cancel(err.code());
}

void Client::on_timer () {
    cancel(std::errc::operation_timed_out);
}

void Client::on_read (string& _buf, const CodeError& err) {
    if (err) return cancel(err.code());

    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING
    using ParseState = protocol::http::ResponseParser::State;

    auto result = _response_parser->parse_first(buf);
    if (!result.state) return cancel(errc::parser_error);

    if (!result.response->is_valid()) return;
    if (_timer) _timer->stop();

    auto req = _request;
    auto res = static_pointer_cast<Response>(result.response);

    if (result.state == ParseState::done) {
        _request.reset();
        _response.reset();
        if (result.position < buf.length()) {
            panda_log_warning("got garbage after http response");
            Tcp::reset();
        }
        if (_pool) _pool->putback(this);
        request->handle_response(req, res, {});
    } else {
        _response = res;
        request->handle_response(req, res, {});
    }
}

void Connection::detach(RequestSP request) {
    _EDEBUGTHIS("detach");
    connect_callback.remove_object(function_details::tmp_abstract_function(&Request::on_connect, request));
    inactivity_timer_->start(inactivity_timeout_);
}

void Connection::close() {
    _EDEBUGTHIS("close");
    connected_ = false;
    shutdown();
    disconnect();
    close_callback(this);
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

void Connection::on_stream_error(const CodeError& err) {
    _EDEBUGTHIS("on_stream_error: %s", err.what());
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
}

}}}
