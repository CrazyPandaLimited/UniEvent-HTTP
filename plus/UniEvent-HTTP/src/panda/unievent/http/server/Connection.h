#pragma once

#include <cstdint>
#include <deque>

#include <panda/refcnt.h>
#include <panda/unievent/Loop.h>
#include <panda/unievent/Stream.h>
#include <panda/unievent/TCP.h>
#include <panda/CallbackDispatcher.h>
#include <panda/protocol/http/RequestParser.h>
#include <panda/protocol/http/Response.h>

#include "../common/Defines.h"
#include "../common/Response.h"

namespace panda { namespace unievent { namespace http { namespace server {

class Connection : public TCP {
public:
    virtual ~Connection();

    Connection(Server* server, uint64_t id);

    uint64_t id() const { return id_; }

    virtual void run();

    virtual void close(uint16_t code, string payload);

    Server* server() const { return server_; }

    CallbackDispatcher<void(ConnectionSP, protocol::http::RequestSP, ResponseSP&)> request_callback;
    CallbackDispatcher<void(ConnectionSP, const CodeError*)> stream_error_callback;
    CallbackDispatcher<void(ConnectionSP, const string&)> any_error_callback;
    CallbackDispatcher<void(ConnectionSP, uint16_t, string)> close_callback;

private:
    virtual void on_request(protocol::http::RequestSP msg);
    virtual void on_stream_error(const CodeError* err);
    virtual void on_any_error(const string& err);

    virtual void on_eof() override;

    void close_tcp();

    void on_read(string& buf, const CodeError* err) override;

    void write_chunk(const string& buf, bool is_last);

private:
    Server* server_;
    uint64_t id_;
    uint64_t request_counter_;
    uint64_t part_counter_;
    bool alive_;
    iptr<protocol::http::RequestParser> request_parser_;
    std::deque<ResponseSP> responses_;
};

}}}} // namespace panda::unievent::http::server
