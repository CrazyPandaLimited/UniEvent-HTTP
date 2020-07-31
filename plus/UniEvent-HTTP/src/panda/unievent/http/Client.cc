#include "Pool.h"
#include "Client.h"
#include <ostream>
#include <panda/log.h>
#include <panda/unievent/socks.h>

#define HOLD_ON(this) ClientSP hold = this; (void)hold

namespace panda { namespace unievent { namespace http {

using namespace panda::unievent::socks;

const string DEFAULT_UA = "Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/47.0.2526.111 Safari/537.36 UniEvent-HTTP/1.0";


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

Client::Client (const LoopSP& loop) : Tcp(loop), _netloc({"", 0, nullptr, {}}) {
    Tcp::event_listener(this);
}

Client::Client (Pool* pool) : Client(pool->loop()) {
    _pool = pool;
}

void Client::request (const RequestSP& request) {
    if (_request) throw HttpError("client supports only one request at a time");
    if (request->_client) {
        panda_log_error("request with a present client: " << request->_client);
        throw HttpError("request is already in progress");
    }
    request->check();
    panda_log_info("request:\n" << request->to_string());

    request->_client = this;
    if (!request->uri->scheme()) request->uri->scheme("http");
    if (!request->chunked || request->body.length()) request->_transfer_completed = true;

    auto netloc = request->netloc();

    if (!connected() || _netloc != netloc) {
        panda_log_info("connecting to " << netloc);
        if (connected()) drop_connection();
        filters().clear();

        if (request->uri->secure()) {
            if (request->ssl_ctx) Tcp::use_ssl(request->ssl_ctx);
            else                  Tcp::use_ssl();
        }

        if (request->proxy) {
            auto uri = request->proxy;
            if (uri->scheme() == "socks5") {
                SocksSP socks = new Socks(uri->host(), uri->port(), uri->user(), uri->password());
                use_socks(this, socks);
            }
            else throw HttpError("client supports only socks5 protocol for proxy");
        }

        if (request->tcp_nodelay) set_nodelay(true);
        _netloc = std::move(netloc);
        connect(_netloc.host, _netloc.port, request->timeout, request->tcp_hints);
    }

    // this code should be after connect, because in case of connect timeout, timer inside Tcp class must react first to mark multiDNS address as bad
    if (request->timeout) {
        if (!request->_timer) request->_timer = new Timer(loop());
        request->_timer->event_listener(this);
        if (!request->_timer->active()) request->_timer->once(request->timeout); // if active it may be redirected request
    }

    Tcp::weak(false);
    _request = request;

    using namespace panda::protocol::http;
    if (request->compression_prefs == static_cast<std::uint8_t>(Compression::IDENTITY) && !request->headers.has("Accept-Encoding")) {
        request->allow_compression(Compression::GZIP);
    }

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
    auto v = req->final_chunk();
    write(v.begin(), v.end());
}

void Client::cancel (const ErrorCode& err) {
    if (!_request) return;
    panda_log_info("cancel with err = " << err);
    _parser.reset();

    if (_in_redirect) _redirect_canceled = true;

    finish_request(err);
}

void Client::on_connect (const ErrorCode& err, const ConnectRequestSP&) {
    if (_request && err) cancel(nest_error(errc::connect_error, err));
}

void Client::on_write (const ErrorCode& err, const WriteRequestSP&) {
    if (_request && err) cancel(err);
}

void Client::on_timer (const TimerSP& t) {
    HOLD_ON(this);
    assert(_request);
    assert(_request->_timer == t);
    cancel(make_error_code(std::errc::timed_out));
}

void Client::on_read (string& buf, const ErrorCode& err) {
    if (err) return cancel(err);
    panda_log_debug("read (" << buf.size() << " bytes):\n" << buf);
    while (buf) {
        if (!_parser.context_request()) {
            panda_log_notice("unexpected buffer: " << buf);
            drop_connection();
            break;
        }

        auto result = _parser.parse_shift(buf);
        _response = static_pointer_cast<Response>(result.response);
        _response->_is_done = result.state >= State::done;

        if (result.error) return cancel(result.error);

        if (result.state <= State::headers) {
            panda_log_debug("got part, headers not finished");
            return;
        }

        if (result.state != State::done) {
            panda_log_debug("got part, body not finished");
            if (_response->code == 100) continue;
            if (_request->follow_redirect && is_redirect(_response->code)) continue;
            _request->partial_event(_request, _response, {});
            continue;
        }

        analyze_request();
    }
}

void Client::analyze_request () {
    panda_log_info("analyze, code = " << _response->code);
    /* nullptr as we don't want to compression be applied */
    panda_log_debug("analyze, (uncompressed) response = " << _response->to_string(nullptr));

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

        // record prev context
        RedirectContextSP redirect_ctx(new RedirectContext{prev_uri,  _request->ssl_ctx,  _request->cookies });
        auto& headers = _request->headers.fields;
        for (auto it = headers.begin(); it != headers.end();) {
            auto& name = it->name;
            if ((name == "Authorization" ) || (name == "Cookie" ) || (name == "Host") || (name == "Referer")) {
                redirect_ctx->removed_headers.add(name, it->value);
                it = headers.erase(it);
            } else ++it;
        }
        _request->ssl_ctx.reset();
        _request->cookies.clear();
        _request->uri = uri;

        try {
            _in_redirect = true;
            _request->redirect_event(_request, _response, redirect_ctx);
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

        if (_response->code == 303) {
            _request->_method = Request::Method::GET;
            _request->body.clear();
        }

        panda_log_info("following redirect: " << prev_uri->to_string() << " -> " << uri->to_string() << " (" << _request->_redirection_counter << " of " << _request->redirection_limit << ")");
        auto netloc = _request->netloc();

        auto req = std::move(_request);
        auto res = std::move(_response);
        req->_client = nullptr;
        if (!res->keep_alive()) Tcp::reset();

        if (_pool && (netloc.host != _netloc.host || netloc.port != _netloc.port)) {
            panda_log_debug("using pool");
            _last_activity_time = loop()->now();
            _pool->putback(this);
            Tcp::weak(true);
            _pool->request(req);
        } else {
            panda_log_debug("using self again");
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

void Client::finish_request (const ErrorCode& _err) {
    auto req = std::move(_request);
    auto res = std::move(_response);

    auto err = _err;
    if (!err && !req->_transfer_completed) err = errc::transfer_aborted;

    if (err || !res->keep_alive()) drop_connection();
    else Tcp::weak(true);

    req->_redirection_counter = 0;
    req->_client = nullptr;
    req->_transfer_completed = false;

    if (req->_timer) {
        req->_timer->event_listener(nullptr);
        req->_timer->stop();
    }

    _last_activity_time = loop()->now();
    if (_pool) _pool->putback(this);

    if (!res) res = static_pointer_cast<Response>(req->new_response()); // ensure we pass non-null response even for earliest errors

    panda_log_info("finish_request 1, client: " << req->_client);
    req->partial_event(req, res, err);
    panda_log_info("finish_request 2, client: " << req->_client);
    req->response_event(req, res, err);
    panda_log_info("finish_request 3, client: " << req->_client);
}

void Client::on_eof () {
    panda_log_info("got eof");
    if (!_request) {
        Tcp::reset();
        return;
    }

    auto result = _parser.eof();
    _response = static_pointer_cast<Response>(result.response);
    _response->_is_done = true;

    if (result.error) {
        cancel(result.error);
    } else {
        analyze_request();
    }
}

}}}
