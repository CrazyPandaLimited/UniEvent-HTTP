#include "ServerConnection.h"
#include "Server.h"

namespace panda { namespace unievent { namespace http {

ServerConnection::ServerConnection (Server* server, uint64_t id) : Tcp(server->loop()), _id(id), server(server), parser(this), closing() {
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

    if (closing) {
        panda_log_debug("got data in closing state");
        return;
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
            server->request_event(req);
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

    if (!res->code) res->code = 200;
    if (!res->headers.has_field("Date")) res->headers.date(server->date_header_now());

    decltype(res->body.parts) tmp_chunks;
    if (res->chunked && !res->_completed && res->body.length()) {
        tmp_chunks = std::move(res->body.parts);
        res->body.parts.clear();
    }

    auto v = res->to_vector(req);
    write(v.begin(), v.end());
    printf("========================= writing: =====================\n");
    for (auto it = v.begin(); it != v.end(); ++it) {
        printf("%s", it->c_str());
    }
    printf("\n=====================================\n");

    if (!res->keep_alive()) {
        closing = true;
        if (requests.size() > 1) { // drop all pipelined requests
            requests.pop_front();
            drop_requests(make_error_code(std::errc::connection_reset));
            requests.push_front(req);
        }
    }

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
        printf("========================= writing chunk: =====================\n");
        for (auto it = v.begin(); it != v.end(); ++it) {
            printf("%s", it->c_str());
        }
        printf("\n=====================================\n");
        return;
    }

    res->body.parts.push_back(chunk);
}

void ServerConnection::send_final_chunk (const ServerResponseSP& res) {
    assert(requests.size());
    res->_completed = true;
    if (requests.front()->_response != res) return;

    write(res->final_chunk());
    printf("writing final chunk\n");
    finish_request();
}

void ServerConnection::finish_request () {
    cleanup_request();
    if (closing) {
        assert(!requests.size());
        close({}, true);
        return;
    }
    if (requests.size() && requests.front()->_response) write_next_response();
}

void ServerConnection::cleanup_request () {
    auto req = requests.front();
    req->_connection = nullptr;
    requests.pop_front();
}

void ServerConnection::on_write (const CodeError& err, const WriteRequestSP&) {
    if (!err) return;
    panda_log_info("write error: " << err.whats());
    close(make_error_code(std::errc::connection_reset), false);
}

void ServerConnection::on_eof () {
    panda_log_debug("eof");
    close(make_error_code(std::errc::connection_reset), true);
}

void ServerConnection::drop_requests (const std::error_code& err) {
    while (requests.size()) {
        auto req = requests.front();
        // remove request from pool first, because no one listen for responses,
        // we need request/response objects to completely ignore any calls to respond(), send_chunk(), end_chunk()
        cleanup_request();
        if (req->_state != State::done) {
            if (req->_partial) req->partial_event(req, err);
            else               server->error_event(req, err);
        } else {
            if (req->_response && req->_response->_completed) {} // nothing to do. user already processed and forgot this request
            else req->drop_event(req, err);
        }
    }
}

void ServerConnection::close (const std::error_code& err, bool soft) {
    panda_log_info("close: " << err);
    event_listener(nullptr);
    soft ? disconnect() : reset();
    drop_requests(err);
    server->handle_eof(this);
}

static ServerResponseSP default_error_response (int code) {
    ServerResponseSP ret = new ServerResponse(code);

    string body(600);
    body +=
        "<html>\n"
        "<head><title>";
    body += ret->full_message();
    body +=
        "</title></head>\n"
        "<body bgcolor=\"white\">\n"
        "<center><h1>";
    body += ret->full_message();
    body +=
        "</h1></center>\n"
        "<hr><center>UniEvent-HTTP</center>\n"
        "</body>\n"
        "</html>\n";
    for (int i = 0; i < 6; ++i) body += "<!-- a padding to disable MSIE and Chrome friendly error page -->\n";

    ret->body = body;

    return ret;
}

void ServerConnection::request_error (const ServerRequestSP& req, const std::error_code& err) {
    read_stop();
    if (req->_partial) req->partial_event(req, err);
    else               server->error_event(req, err);
    if (!req->_response) respond(req, default_error_response(400));
}

}}}
