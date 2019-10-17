#pragma once
#include <deque>
#include "ServerRequest.h"
#include <panda/unievent/Tcp.h>
#include <panda/protocol/http/RequestParser.h>

namespace panda { namespace unievent { namespace http {

struct Server;

struct ServerConnection : Tcp, private ITcpSelfListener, private protocol::http::IRequestFactory {
    ServerConnection (Server*, uint64_t id);

    ~ServerConnection ();

    uint64_t id () const { return _id; }

//    virtual void close(uint16_t code, string payload);
//    Server* server() const { return server_; }
private:

//    virtual void on_request(protocol::http::RequestSP msg);
//    virtual void on_stream_error(const CodeError& err);
//    virtual void on_any_error(const string& err);
//
//    virtual void on_eof() override;
//
//    void close_tcp();
//
//    void write_chunk(const string& buf, bool is_last);

private:
    friend ServerRequest; friend ServerResponse;

    using RequestParser = protocol::http::RequestParser;
    using RequestList   = std::deque<ServerRequestSP>;

    uint64_t      _id;
    Server*       server;
    RequestParser parser;
    RequestList   requests;

    protocol::http::RequestSP create_request () { return new ServerRequest(this); }

    void on_read  (string&, const CodeError&) override;
    void on_write (const CodeError&, const WriteRequestSP&) override;
    void on_eof   () override;

    void request_error (const ServerRequestSP&, const std::error_code& err);

    void respond             (const ServerRequestSP&, const ServerResponseSP&);
    void write_next_response ();
    void send_chunk          (const ServerResponseSP&, const string& chunk);
    void end_chunk           (const ServerResponseSP&);
    void finish_request      ();
    void close               (const std::error_code&);
};
using ServerConnectionSP = iptr<ServerConnection>;

}}}
