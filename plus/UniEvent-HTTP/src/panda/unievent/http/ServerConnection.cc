#include "ServerConnection.h"
#include "Server.h"

//#include <panda/string.h>
//#include <panda/function.h>
//#include <panda/protocol/http/Request.h>
//#include <panda/protocol/http/Response.h>
//
//#include "Server.h"
//#include "../common/Dump.h"

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
            req->partial_event(req, {});
            return;
        }

        if (result.state == State::done) {
            req->receive_event(req);
            server->receive_event(req);
        }
    }
}

void ServerConnection::respond (const RequestSP& req, const ResponseSP& res) {
    assert(req->_connection == this);
    if (req->_response) throw HttpError("double response for request given");
    req->_response = res;
    res->_request = req;
    if (!res->chunked || res->body.length()) res->_completed = true;
    if (_requests.front() == req) write_next_response();
}

void ServerConnection::write_next_response () {
    auto req = _requests.front();
    auto res = req->_response;

    if (!res->code) {
        res->code = 200;
        res->message = "OK";
    }

    if (!res->headers.has_field("Date")) res->headers.date(_server->date_header_now());

    std::vector<string> tmp_chunks;
    if (res->chunked && !res->_completed && res->body.parts.size()) tmp_chunks = std::move(res->body.parts);

    auto v = res->to_vector(req);
    write(v.begin(), b.end());

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
    assert(_requests.size());
    if (!chunk) return;

    if (_requests.front()->_response == res) {
        auto v = res->make_chunk(chunk);
        write(v.begin(), v.end());
        return;
    }

    res->body.parts.push_back(chunk);
}

void ServerConnection::end_chunk (const ServerResponseSP& res) {
    assert(_requests.size());
    res->_completed = true;
    if (_requests.front()->_response != res) return;

    write(res->end_chunk());
    finish_request();
}

void ServerConnection::finish_request () {
    auto req = _requests.front();
    auto res = req->_response;
    assert(res->_completed);

    req->_connection = nullptr;
    _requests.pop_front();
    if (_requests.front()._response && Tcp::connected()) write_next_response();
}

void ServerConnection::on_write (const CodeError& err, const WriteRequestSP&) {
    if (!err) return;
    panda_log_info("write error: " << err);
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
    while (_requests.size()) {
        auto req = _requests.front();
        // remove request from pool first, because no one listen for responses,
        // we need request/response objects to completely ignore any calls to respond(), send_chunk(), end_chunk()
        finish_request(req);
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

//void Connection::close(uint16_t, string) {
//    server_->remove_connection(this);
//}
//
//void Connection::on_request(protocol::http::RequestSP request) {
//    _EDEBUGTHIS("on_request");
//
//    //if (Log::should_log(logger::DEBUG, _panda_code_point_)){
//        //Log logger = Log(_panda_code_point_, logger::DEBUG);
//        //logger << "on_request: payload=\n";
//        //logger << request;
//    //}
//
//    ResponseSP response;
//    request_callback(this, request, response);
//
//    response->write_callback.add(function_details::tmp_abstract_function(&Connection::write_chunk, iptr<Connection>(this)));
//
//    if(response->chunked()) {
//        responses_.emplace_back(response);
//    }
//
//    auto response_vector = to_vector(response);
//    //for(auto part : response_vector)
//        //panda_log_debug("on_request" << part);
//    //panda_log_debug("on_request response: " << response);
//    write(response_vector.begin(), response_vector.end());
//}
//
//void Connection::write_chunk(const string& buf, bool is_last) {
//    _EDEBUGTHIS("write_chunk, is_last=%d", is_last);
//    std::vector<panda::string> chunk_with_length = { string::from_number(buf.length(), 16) + "\r\n", buf, "\r\n" };
//    if(is_last) {
//        chunk_with_length.push_back("0\r\n\r\n");
//    }
//    write(begin(chunk_with_length), end(chunk_with_length));
//}
//
//void Connection::on_stream_error(const CodeError& err) {
//    _EDEBUGTHIS("on_stream_error");
//    stream_error_callback(this, err);
//    //on_any_error(err.what());
//}
//
//void Connection::on_any_error(const string& err) {
//    _EDEBUGTHIS("on_any_error");
//    any_error_callback(this, err);
//}
//
//void Connection::on_eof() {
//    _EDEBUGTHIS("on_eof");
//}
//
//void Connection::close_tcp() {
//    shutdown();
//    disconnect();
//}

}}}
