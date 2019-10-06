//#include "Connection.h"
//
//#include <panda/string.h>
//#include <panda/function.h>
//#include <panda/protocol/http/Request.h>
//#include <panda/protocol/http/Response.h>
//
//#include "Server.h"
//#include "../common/Dump.h"
//
//namespace panda { namespace unievent { namespace http { namespace server {
//
//Connection::~Connection() { _EDTOR(); }
//
//Connection::Connection(Server* server, uint64_t id)
//        : Tcp(server->loop())
//        , server_(server)
//        , id_(id)
//        , request_parser_(make_iptr<protocol::http::RequestParser>()) {
//    _ECTOR();
//    event_listener(this);
//}
//
//void Connection::run() { read_start(); }
//
//void Connection::on_read(string& _buf, const CodeError& err) {
//    _EDEBUGTHIS("on_read %zu", _buf.size());
//    if(err) {
//        return on_stream_error(err);
//    }
//
//    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING
//    using ParseState = protocol::http::RequestParser::State;
//
//    for(auto pos = request_parser_->parse(buf); pos->state != ParseState::not_yet; ++pos) {
//        _EDEBUGTHIS("parser state: %d %zu", pos->state.value_or(ParseState::error), pos->position);
//        if(!pos->state) {
//            _EDEBUGTHIS("parser failed: %zu", pos->position);
//            on_any_error("parser failed");
//            return;
//        }
//        else if(pos->state == ParseState::got_header) {
//            _EDEBUGTHIS("got header: %zu", pos->position);
//        }
//        else if(pos->state == ParseState::done) {
//            _EDEBUGTHIS("got body: %zu", pos->position);
//        }
//
//        if(pos->request->is_valid()) {
//            on_request(pos->request);
//        }
//
//        if(pos->position >= buf.size()) {
//            return;
//        }
//    }
//}
//
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
//
//}}}} // namespace panda::unievent::http::server
