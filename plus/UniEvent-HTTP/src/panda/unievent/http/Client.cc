#include "Pool.h"
#include "Client.h"
#include <ostream>

namespace panda { namespace unievent { namespace http {

Client::Client (const LoopSP& loop) : Tcp(loop), _pool(), _netloc({"", 0}), _last_activity_time(0) {
    Tcp::event_listener(this);
}

Client::Client (Pool* pool) : Client(pool->loop()) {
    _pool = pool;
}

Client::~Client () {
    cancel();
}

void Client::request (const RequestSP& request) {
    if (_request) throw HttpError("client supports only one request at a time");
    if (request->_client) throw HttpError("request is already in progress");
    _request = request;
    request->_client = this;

    if (request->timeout) {
        if (!request->_timer) request->_timer = new Timer(loop());
        request->_timer->event_listener(this);
        if (!request->_timer->active()) request->_timer->once(request->timeout); // if active it may be redirected request
    }

    auto netloc = request->netloc();

    if (!connected() || _netloc.host != netloc.host || _netloc.port != netloc.port) {
        if (_netloc.host) { // not first use
            Tcp::clear();
            Tcp::event_listener(this);
        }
        if (request->uri->secure()) use_ssl(); // filters have been cleared in clear()
        _netloc = std::move(netloc);
        connect(_netloc.host, _netloc.port);
    }

    auto data = request->to_vector();
    _parser.set_request(request);
    read_start();
    write(data.begin(), data.end());
}

void Client::cancel (const std::error_code& err) {
    if (!_request) return;
    drop_connection();
    _parser.reset();
    finish_request(err);
}

void Client::on_connect (const CodeError& err, const ConnectRequestSP&) {
    if (_request && err) cancel(err.code());
}

void Client::on_write (const CodeError& err, const WriteRequestSP&) {
    if (_request && err) cancel(err.code());
}

void Client::on_timer (const TimerSP& t) {
    assert(_request);
    assert(_request->_timer == t);
    cancel(make_error_code(std::errc::timed_out));
}

void Client::on_read (string& _buf, const CodeError& err) {
    if (err) return cancel(err.code());

    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING
    using ParseState = ResponseParser::State;

    auto result = _parser.parse(buf);
    if (!result.state) return cancel(errc::parser_error);
    if (result.state.value() < ParseState::got_header) return;

    if (result.state.value() != ParseState::done) {
        _response = static_pointer_cast<Response>(result.response);
        _request->handle_response(_request, _response, {});
        return;
    }

    if (result.position < buf.length()) {
        panda_log_warn("got garbage after http response");
        drop_connection();
    }

    analyze_request();
}

void Client::analyze_request () {
    switch (_response->code) {
        case 300:
        case 301:
        case 302:
        case 303:
        // disabled for security reasons
        // case 305:
        case 307:
        case 308:
            if (++_request->_redirection_counter > _request->redirection_limit) return cancel(errc::redirection_limit);

            auto uristr = _response->headers.get_field("Location");
            if (!uristr) return cancel(errc::no_redirect_uri);

            URISP uri = new URI(uristr);
            auto prev_uri = _request->uri;
            if (!uri->scheme()) uri->scheme(prev_uri->scheme());
            if (!uri->host()) {
                uri->host(prev_uri->host());
                if (prev_uri->explicit_port()) uri->port(prev_uri->port());
            }

            _request->handle_redirect(_request, _response, uri);

            _request->_original_uri = prev_uri;
            _request->uri = uri;
            auto netloc = _request->netloc();

            auto req = std::move(_request);
            _response.reset();
            if (_pool && (netloc.host != _netloc.host || netloc.port != _netloc.port)) {
                _pool->putback(this);
                _pool->request(req);
            } else {
                request(req);
            }
            return;
    }

    finish_request({});
}

void Client::drop_connection () {
    auto req = std::move(_request); // temporarily remove _request to suppress cancel() from on_connect/on_write with error
    Tcp::reset();
    _request = std::move(req);
}

void Client::finish_request (const std::error_code& err) {
    if (_pool) _pool->putback(this);
    auto req = std::move(_request);
    auto res = std::move(_response);

    req->_redirection_counter = 0;
    req->_client = nullptr;
    if (req->_timer) {
        req->_timer->event_listener(nullptr);
        req->_timer->stop();
    }

    if (!res->keep_alive()) Tcp::reset();

    req->handle_response(req, res, err);
}

void Client::on_eof () {
    if (_request) {
        auto result = _parser.eof();
        if (!result.state) return cancel(make_error_code(std::errc::connection_reset));
        analyze_request();
    }
    else Tcp::reset();
}

}}}
