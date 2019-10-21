#include "ServerConnection.h"
#include "Server.h"

namespace panda { namespace unievent { namespace http {

ServerConnection::ServerConnection (Server* server, uint64_t id) : Tcp(server->loop()), _id(id), server(server), parser(this) {
    Tcp::event_listener(this);
}

ServerConnection::~ServerConnection () {
}

void ServerConnection::on_read (string& _buf, const CodeError& err) {
    if (err) {
        panda_log_info("read error: " << err.whats());
        if (!requests.size() || requests.back()->_state == State::done) requests.emplace_back(new ServerRequest(this));
        requests.back()->_state = State::error;
        return request_error(requests.back(), errc::parse_error);
    }
    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING

    while (buf) {
        auto result = parser.parse_shift(buf);

        auto req = static_pointer_cast<ServerRequest>(result.request);
        if (!requests.size() || requests.back() != req) requests.emplace_back(req);

        req->_state = result.state;

        if (result.error) {
            panda_log_info("parser error: " << result.error);
            return request_error(req, errc::parse_error);
        }

        if (result.state < State::got_header) {
            panda_log_verbose_debug("got part, headers not finished");
            return;
        }

        panda_log_verbose_debug("got part, body finished = " << (result.state == State::done));

        if (!req->_routed) {
            req->_routed = true;
            server->route_event(req);
        }

        if (req->_partial) {
            req->partial_event(req, std::error_code());
            return;
        }

        if (result.state == State::done) {
            req->receive_event(req);
            server->receive_event(req);
        }
    }
}

void ServerConnection::respond (const ServerRequestSP& req, const ServerResponseSP& res) {
    assert(req->_connection == this);
    if (req->_response) throw HttpError("double response for request given");
    req->_response = res;
    res->_request = req;
    if (!res->chunked || res->body.length()) res->_completed = true;
    if (requests.front() == req) write_next_response();
}

void ServerConnection::write_next_response () {
    auto req = requests.front();
    auto res = req->_response;

    if (!res->code) {
        res->code = 200;
        res->message = "OK";
    }

    if (!res->headers.has_field("Date")) res->headers.date(server->date_header_now());

    decltype(res->body.parts) tmp_chunks;
    if (res->chunked && !res->_completed && res->body.parts.size()) tmp_chunks = std::move(res->body.parts);

    auto v = res->to_vector(req);
    write(v.begin(), v.end());

    if (!res->_completed) {
        if (tmp_chunks.size()) {
            for (auto& chunk : tmp_chunks) {
                auto v = res->make_chunk(chunk);
                write(v.begin(), v.end());
            }
        }
        return;
    }

    finish_request();
}

void ServerConnection::send_chunk (const ServerResponseSP& res, const string& chunk) {
    assert(requests.size());
    if (!chunk) return;

    if (requests.front()->_response == res) {
        auto v = res->make_chunk(chunk);
        write(v.begin(), v.end());
        return;
    }

    res->body.parts.push_back(chunk);
}

void ServerConnection::finalize_chunk (const ServerResponseSP& res) {
    assert(requests.size());
    res->_completed = true;
    if (requests.front()->_response != res) return;

    write(res->final_chunk());
    finish_request();
}

void ServerConnection::finish_request () {
    auto req = requests.front();
    auto res = req->_response;
    assert(res->_completed);

    req->_connection = nullptr;
    requests.pop_front();
    if (requests.front()->_response && Tcp::connected()) write_next_response();
}

void ServerConnection::on_write (const CodeError& err, const WriteRequestSP&) {
    if (!err) return;
    panda_log_info("write error: " << err.whats());
    event_listener(nullptr);
    reset();
    close(make_error_code(std::errc::connection_reset));
}

void ServerConnection::on_eof () {
    panda_log_debug("eof");
    event_listener(nullptr);
    disconnect();
    close(make_error_code(std::errc::connection_reset));
    server->handle_eof(this);
}

void ServerConnection::close (const std::error_code& err) {
    while (requests.size()) {
        auto req = requests.front();
        // remove request from pool first, because no one listen for responses,
        // we need request/response objects to completely ignore any calls to respond(), send_chunk(), end_chunk()
        finish_request();
        if (req->_state != State::done) {
            if (req->_partial) req->partial_event(req, err);
            else               server->error_event(req, err);
        } else {
            if (req->_response && req->_response->_completed) {} // nothing to do. user already processed and forgot this request
            else req->drop_event(req, err);
        }
    }
}

void ServerConnection::request_error (const ServerRequestSP& req, const std::error_code& err) {
    read_stop();
    if (req->_partial) req->partial_event(req, err);
    else               server->error_event(req, err);
    if (!req->_response) respond(req, new ServerResponse(400, "Bad Request", Header(), Body(), HttpVersion::any, false));
}

}}}
