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

ServerConnection::ServerConnection (Server* server, uint64_t id) : Tcp(server->loop()), _server(server), _id(id), _request_parser(this) {
    Tcp::event_listener(this);
}

ServerConnection::~ServerConnection () {
}

void ServerConnection::on_read (string& _buf, const CodeError& err) {
    if (err) {
        panda_log_info("read error: " << err);
        if (!_requests.size() || _requests.back().state == Request::State::done) _request.emplace_back(new Request());
        return request_error(_request.back(), err.code());
    }
    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING

    while (buf) {
        auto result = _parser.parse_shift(buf);

        auto req = static_pointer_cast<Request>(result.request);
        if (!_requests.size() || _requests.back().request != req) _request.emplace_back(req);
        RequestData& rd = _requests.back();

        if (!result.state) {
            panda_log_info("parser error: " << result.state.error());
            return request_error(rd, result.state.error());
        }
        rd.state = result.state.value();

        if (rd.state < Request::State::got_header) {
            panda_log_verbose_debug("got part, headers not finished");
            return;
        }


        rd.partial_called = true;
        _server->handle_partial(req, rd.state, {}, this);

        if (rd.state != Request::State::done) {
            panda_log_verbose_debug("got part, body not finished");
            return;
        }

        _server->handle_request(req, {}, this);
    }
}

void ServerConnection::respond (const RequestSP& req, const ResponseSP& res) {
    auto rd = find_data(req);
    if (!rd || rd->response) throw HttpError("double response for request given");

    if (!res->code) {
        res->code = 200;
        res->message = "OK";
    }

    if (!res->headers.has_field("Date")) res->headers.date(_server->get_date_header());

    rd->response = res;


}

void ServerConnection::request_error (RequestData& rd, const std::error_code& err) {
    read_stop();
    auto req = rd.request;
    if (rd.partial_called) _server->handle_partial(req, rd.state, err, this);
    if (!responded(req)) respond(req, new Response(400, "Bad Request", Header(), Body(), "", false));
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
