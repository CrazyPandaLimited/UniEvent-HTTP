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

ServerConnection::ServerConnection (Server* server, uint64_t id) : Tcp(server->loop()), server(server), _id(id), parser(this) {
    Tcp::event_listener(this);
}

ServerConnection::~ServerConnection () {
}

void ServerConnection::on_read (string& _buf, const CodeError& err) {
    if (err) {
        panda_log_info("read error: " << err.whats());
        if (!requests.size() || requests.back()->state == State::done) requests.emplace_back(new ServerRequest(this));
        requests.back()->_state = State::error;
        return request_error(requests.back(), err.code());
    }
    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING

    while (buf) {
        auto result = parser.parse_shift(buf);

        auto req = static_pointer_cast<ServerRequest>(result.request);
        if (!requests.size() || requests.back() != req) requests.emplace_back(req);

        if (!result.state) {
            req->_state = State::error;
            panda_log_info("parser error: " << result.state.error());
            return request_error(req, result.state.error());
        }
        req->_state = result.state.value();

        if (req->_state < State::got_header) {
            panda_log_verbose_debug("got part, headers not finished");
            return;
        }

        req->partial_called = true;
        server->handle_partial(req, {}, this);

        if (req->_state != State::done) {
            panda_log_verbose_debug("got part, body not finished");
            return;
        }

        server->handle_request(req, this);
    }
}

void ServerConnection::respond (const RequestSP& req, const ResponseSP& res) {
    assert(req->_connection == this);
    if (req->_response) throw HttpError("double response for request given");
    req->_response = res;
    res->_request = req;
    if (!res->chunked || res->body.length()) res->_completed = true;
    if (_requests.front() == req) write_response();
}

void ServerConnection::write_response () {
    auto req = _requests.front();
    auto res = req->_response;

    if (!res->code) {
        res->code = 200;
        res->message = "OK";
    }

    if (!res->headers.has_field("Date")) res->headers.date(_server->date_header_now());

    std::vector<string> tmp_chunks;
    if (!res->_completed && res->body.parts.size()) tmp_chunks = std::move(res->body.parts);

    auto v = res->to_vector(req);
    write(v.begin(), b.end());

    if (!res->_completed) {
        if (tmp_chunks.size()) res->body.parts = std::move(tmp_chunks);
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

    if (_requests.front()->_response == res) {
        write(res->end_chunk());
        finish_request();
        return;
    }

    res->_completed = true;
}

void ServerConnection::finish_request () {
    auto req = _requests.front();
    auto res = req->_response;
    assert(res->_completed);

    req->_connection = nullptr;
    _requests.pop_front();
    if (_requests.front()._response) write_response();
}

void ServerConnection::on_write (const CodeError& err, const WriteRequestSP&) {
    if (!err) return;
    panda_log_info("write error: " << err);
}

void ServerConnection::request_error (const ServerRequestSP& req, const std::error_code& err) {
    read_stop();
    if (req->partial_called) server->handle_partial(req, err, this);
    if (!req->response) respond(req, new ServerResponse(400, "Bad Request", Header(), Body(), HttpVersion::any, false));
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
