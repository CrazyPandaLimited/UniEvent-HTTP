#include "Connection.h"

#include <panda/log.h>
#include <panda/string.h>
#include <panda/function.h>
#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>

#include "Server.h"
#include "../common/Dump.h"

namespace panda { namespace unievent { namespace http { namespace server {

Connection::~Connection() {
    panda_log_debug("dtor");
}

Connection::Connection(Server* server, uint64_t id) : 
    TCP(server->loop()),
    server_(server),
    id_(id),
    request_counter_(0),
    part_counter_(0),
    alive_(true),
    request_parser_(make_iptr<protocol::http::RequestParser>())
{
    panda_log_debug("ctor connection id = " << id_);
}

void Connection::run() { read_start(); }

void Connection::on_read(string& _buf, const CodeError* err) {
    panda_log_debug("on_read " << _buf.size());
    
    string buf = string(_buf.data(), _buf.length()); // TODO - REMOVE COPYING

    if(err) { 
        return on_stream_error(err); 
    }

#ifdef ENABLE_DUMP_SERVER_MESSAGES
    dump_message(server_->config.dump_file_prefix, request_counter_, part_counter_++, buf);
#endif

    for(auto pos = request_parser_->parse(buf); pos->state != protocol::http::RequestParser::State::not_yet; ++pos) {
        panda_log_debug("parser state: " << static_cast<int>(pos->state) << " " << pos->position);

        if(pos->state == protocol::http::RequestParser::State::failed) {
            panda_log_warn("parser failed: " << pos->position);
            on_any_error("parser failed");
            return;
        }
        else if(pos->state == protocol::http::RequestParser::State::got_header) {
            panda_log_debug("got header: " << pos->position);
        }
        else if(pos->state == protocol::http::RequestParser::State::got_body) {
            panda_log_debug("got body: " << pos->position);
        }

        if(pos->request->is_valid()) {
            on_request(pos->request);
        }

        if(pos->position >= buf.size()) {
            return;
        }
    }
}

void Connection::close(uint16_t, string) {
    server_->remove_connection(this);
}

void Connection::on_request(protocol::http::RequestSP request) {
    panda_log_debug("on_request " << &request);

    //if (Log::should_log(logger::DEBUG, _panda_code_point_)){
        //Log logger = Log(_panda_code_point_, logger::DEBUG);
        //logger << "on_request: payload=\n";
        //logger << request;
    //}
    
    ResponseSP response;
    request_callback(this, request, response);

    response->write_callback.add(function_details::tmp_abstract_function(&Connection::write_chunk, iptr<Connection>(this)));

    if(response->chunked()) {
        responses_.emplace_back(response);
    }  

    auto response_vector = to_vector(response);
    //for(auto part : response_vector)
        //panda_log_debug("on_request" << part);
    //panda_log_debug("on_request response: " << response);
    write(response_vector.begin(), response_vector.end());
}

void Connection::write_chunk(const string& buf, bool is_last) {
    panda_log_debug("write_chunk, is_last=" << is_last);
    std::vector<panda::string> chunk_with_length = { string::from_number(buf.length(), 16) + "\r\n", buf, "\r\n" };
    if(is_last) {
        chunk_with_length.push_back("0\r\n\r\n");
    } 
    write(begin(chunk_with_length), end(chunk_with_length));
}

void Connection::on_stream_error(const CodeError* err) {
    //panda_log_info("on_stream_error: " << err.what());
    stream_error_callback(this, err);
    //on_any_error(err.what());
}

void Connection::on_any_error(const string& err) {
    panda_log_warn(err);
    any_error_callback(this, err);
}

void Connection::on_eof() {
    panda_log_info("on_eof");
    TCP::on_eof();
}

void Connection::close_tcp() {
    shutdown();
    disconnect();
}

}}}} // namespace panda::unievent::http::server
