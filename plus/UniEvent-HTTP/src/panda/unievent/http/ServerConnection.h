#pragma once
#include <deque>
#include "ServerRequest.h"
#include <panda/unievent/Tcp.h>
#include <panda/protocol/http/RequestParser.h>

namespace panda { namespace unievent { namespace http {

struct Server;

struct ServerConnection : Tcp, private ITcpSelfListener, private protocol::http::RequestParser::IFactory {
    struct IFactory {
        virtual ServerRequestSP new_request (ServerConnection*) = 0;
    };

    struct Config {
        uint32_t  idle_timeout;
        uint64_t  max_keepalive_requests;
        size_t    max_headers_size;
        size_t    max_body_size;
        IFactory* factory;
    };

    ServerConnection (Server*, uint64_t id, const Config&);

    ~ServerConnection () {}

    uint64_t id () const { return _id; }

    void start ();
    void close (const ErrorCode&, bool soft = false);
    void graceful_stop ();

private:
    friend ServerRequest; friend ServerResponse;

    using RequestParser = protocol::http::RequestParser;
    using Requests      = std::deque<ServerRequestSP>;

    Server*       server;
    uint64_t      _id;
    IFactory*     factory;
    RequestParser parser;
    Requests      requests;
    uint64_t      requests_processed = 0;
    uint32_t      idle_timeout;
    uint64_t      max_keepalive_requests;
    TimerSP       idle_timer;
    bool          closing = false;
    bool          stopping = false;

    protocol::http::RequestSP new_request () override;

    void on_read  (string&, const ErrorCode&) override;
    void on_write (const ErrorCode&, const WriteRequestSP&) override;
    void on_eof   () override;

    void request_error (const ServerRequestSP&, const ErrorCode& err);

    void respond             (const ServerRequestSP&, const ServerResponseSP&);
    void write_next_response ();
    void send_continue       (const ServerRequestSP&);
    void send_chunk          (const ServerResponseSP&, const string& chunk);
    void send_final_chunk    (const ServerResponseSP&, const string& chunk);
    void finish_request      ();
    void cleanup_request     ();
    void drop_requests       (const ErrorCode&);

    ServerResponseSP default_error_response (int code);
};
using ServerConnectionSP = iptr<ServerConnection>;

}}}
