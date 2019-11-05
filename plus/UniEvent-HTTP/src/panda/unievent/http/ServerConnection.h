#pragma once
#include <deque>
#include "ServerRequest.h"
#include <panda/unievent/Tcp.h>
#include <panda/protocol/http/RequestParser.h>

namespace panda { namespace unievent { namespace http {

struct Server;

struct ServerConnection : Tcp, private ITcpSelfListener, private protocol::http::IRequestFactory {
    struct Config {
        uint32_t idle_timeout;
    };

    ServerConnection (Server*, uint64_t id, const Config&);

    ~ServerConnection () {}

    uint64_t id () const { return _id; }

    void close (const std::error_code&, bool soft = false);

private:
    friend ServerRequest; friend ServerResponse;

    using RequestParser = protocol::http::RequestParser;
    using RequestList   = std::deque<ServerRequestSP>;

    Server*       server;
    uint64_t      _id;
    Config        conf;
    RequestParser parser;
    RequestList   requests;
    TimerSP       idle_timer;
    bool          closing;

    protocol::http::RequestSP create_request () { return new ServerRequest(this); }

    void on_read  (string&, const CodeError&) override;
    void on_write (const CodeError&, const WriteRequestSP&) override;
    void on_eof   () override;

    void request_error (const ServerRequestSP&, const std::error_code& err);

    void respond             (const ServerRequestSP&, const ServerResponseSP&);
    void write_next_response ();
    void send_chunk          (const ServerResponseSP&, const string& chunk);
    void send_final_chunk    (const ServerResponseSP&);
    void finish_request      ();
    void cleanup_request     ();
    void drop_requests       (const std::error_code&);

    ServerResponseSP default_error_response (int code);
};
using ServerConnectionSP = iptr<ServerConnection>;

}}}