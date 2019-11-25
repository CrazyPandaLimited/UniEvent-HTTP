#include "Pool.h"
#include "Client.h"
#include <ostream>
#include <panda/log.h>

namespace panda { namespace unievent { namespace http {

static inline bool is_redirect (int code) {
    switch (code) {
        case 300:
        case 301:
        case 302:
        case 303:
        // disabled for security reasons
        // case 305:
        case 307:
        case 308:
            return true;
    }
    return false;
}

Client::Client (const LoopSP& loop) : Tcp(loop), _netloc({"", 0}) {
    Tcp::event_listener(this);
}

Client::Client (Pool* pool) : Client(pool->loop()) {
    _pool = pool;
}

void Client::request (const RequestSP& request) {
    if (_request) throw HttpError("client supports only one request at a time");
    if (request->_client) throw HttpError("request is already in progress");
    request->check();
    panda_log_debug("request " << request->uri->to_string());
    panda_log_verbose_debug("\n" << request->to_string());

    request->_client = this;
    if (!request->uri->scheme()) request->uri->scheme("http");
    if (!request->_redirection_counter) request->_original_uri = request->uri;
    if (!request->chunked || request->body.length()) request->_transfer_completed = true;

    if (request->timeout) {
        if (!request->_timer) request->_timer = new Timer(loop());
        request->_timer->event_listener(this);
        if (!request->_timer->active()) request->_timer->once(request->timeout); // if active it may be redirected request
    }

    auto netloc = request->netloc();

    if (!connected() || _netloc.host != netloc.host || _netloc.port != netloc.port) {
        panda_log_debug("connecting to " << netloc);
        if (connected()) drop_connection();
        auto need_secure = request->uri->secure();
        if (need_secure != Tcp::is_secure()) {
            if (need_secure) Tcp::use_ssl();
            else             Tcp::no_ssl();
        }
        _netloc = std::move(netloc);
        connect(_netloc.host, _netloc.port);
    }
    Tcp::weak(false);
    _request = request;

    if (!request->headers.has("User-Agent")) request->headers.add(
        "User-Agent",
        "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.111 Safari/537.36 UniEvent-HTTP/1.0"
    );

    auto data = request->to_vector();
    _parser.set_context_request(request);
    read_start();
    write(data.begin(), data.end());
}

void Client::send_chunk (const RequestSP& req, const string& chunk) {
    assert(_request == req);
    if (!chunk) return;
    auto v = req->make_chunk(chunk);
    write(v.begin(), v.end());
}

void Client::send_final_chunk (const RequestSP& req) {
    assert(_request == req);
    req->_transfer_completed = true;
    write(req->final_chunk());
}

void Client::cancel (const std::error_code& err) {
    if (!_request) return;
    panda_log_debug("cancel with err = " << err);
    _parser.reset();

    if (_in_redirect) _redirect_canceled = true;

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

void Client::on_read (string& buf, const CodeError& err) {
    ClientSP hold = this;
    if (err) return cancel(errc::parse_error);
    panda_log_verbose_debug("read:\n" << buf);

    while (buf) {
        if (!_parser.context_request()) {
            panda_log_info("unexpected buffer: " << buf);
            drop_connection();
            break;
        }

        auto result = _parser.parse_shift(buf);
        _response = static_pointer_cast<Response>(result.response);
        _response->_is_done = result.state >= State::done;

        if (result.error) return cancel(errc::parse_error);

        if (result.state <= State::headers) {
            panda_log_verbose_debug("got part, headers not finished");
            return;
        }

        if (result.state != State::done) {
            panda_log_verbose_debug("got part, body not finished");
            if (_response->code == 100) continue;
            if (_request->follow_redirect && is_redirect(_response->code)) continue;
            _request->partial_event(_request, _response, {});
            continue;
        }

        analyze_request();
    }
}

void Client::analyze_request () {
    panda_log_debug("analyze, code = " << _response->code);

    if (_response->code == 100) {
        _request->continue_event(_request);
        _response.reset();
        return;
    }
    else if (_request->follow_redirect && is_redirect(_response->code)) {
        if (!_request->redirection_limit) return cancel(errc::unexpected_redirect);
        if (++_request->_redirection_counter > _request->redirection_limit) return cancel(errc::redirection_limit);

        auto uristr = _response->headers.get("Location");
        if (!uristr) return cancel(errc::no_redirect_uri);

        URISP uri = new URI(uristr);
        auto prev_uri = _request->uri;
        if (!uri->scheme()) uri->scheme(prev_uri->scheme());
        if (!uri->host()) {
            uri->host(prev_uri->host());
            if (prev_uri->explicit_port()) uri->port(prev_uri->port());
        }

        try {
            _in_redirect = true;
            _request->redirect_event(_request, _response, uri);
            _in_redirect = false;
        }
        catch (...) {
            _in_redirect = false;
            _redirect_canceled = false;
            cancel();
            throw;
        }

        if (_redirect_canceled) {
            _redirect_canceled = false;
            return;
        }

        _request->uri = uri;
        _request->headers.remove("Host"); // will be filled from new uri
        if (_response->code == 303) _request->method = Request::Method::GET;
        panda_log_debug("following redirect: " << prev_uri->to_string() << " -> " << uri->to_string() << " (" << _request->_redirection_counter << " of " << _request->redirection_limit << ")");
        auto netloc = _request->netloc();

        auto req = std::move(_request);
        auto res = std::move(_response);
        req->_client = nullptr;
        if (!res->keep_alive()) Tcp::reset();

        if (_pool && (netloc.host != _netloc.host || netloc.port != _netloc.port)) {
            panda_log_verbose_debug("using pool");
            _pool->putback(this);
            Tcp::weak(true);
            _pool->request(req);
        } else {
            panda_log_verbose_debug("using self again");
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

void Client::finish_request (const std::error_code& _err) {
    if (_pool) _pool->putback(this);
    auto req = std::move(_request);
    auto res = std::move(_response);

    auto err = _err;
    if (!err && !req->_transfer_completed) err = errc::transfer_aborted;

    if (err || !res->keep_alive()) drop_connection();

    Tcp::weak(true);

    req->_redirection_counter = 0;
    req->_client = nullptr;
    req->_transfer_completed = false;

    if (req->_timer) {
        req->_timer->event_listener(nullptr);
        req->_timer->stop();
    }

    req->partial_event(req, res, err);
    req->response_event(req, res, err);
}

void Client::on_eof () {
    panda_log_debug("got eof");
    if (!_request) {
        Tcp::reset();
        return;
    }

    ClientSP hold = this;
    auto result = _parser.eof();
    _response = static_pointer_cast<Response>(result.response);
    _response->_is_done = true;

    if (result.error) {
        cancel(make_error_code(std::errc::connection_reset));
    } else {
        analyze_request();
    }
}

}}}
