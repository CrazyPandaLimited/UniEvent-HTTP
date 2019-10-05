#pragma once
#include "Error.h"
#include "Request.h"
#include <panda/unievent/Tcp.h>
#include <panda/protocol/http/ResponseParser.h>

namespace panda { namespace unievent { namespace http {

struct Pool;
struct Client;
using ClientSP = iptr<Client>;

struct Client : Tcp, private ITcpSelfListener, private ITimerListener {
    Client (const LoopSP&);

    ~Client ();

    void request (const RequestSP&);
    void cancel  (const std::error_code& = make_error_code(std::errc::operation_canceled));

    uint64_t      last_activity_time () const { return _last_activity_time; }
    const NetLoc& last_netloc        () const { return _netloc; }

private:
    using ResponseParser = protocol::http::ResponseParser;

    Pool*          _pool;
    NetLoc         _netloc;
    RequestSP      _request;
    ResponseSP     _response;
    ResponseParser _parser;
    uint64_t       _last_activity_time;

    Client (Pool*);

    void on_connect (const CodeError&, const ConnectRequestSP&) override;
    void on_write   (const CodeError&, const WriteRequestSP&) override;
    void on_read    (string& buf, const CodeError& err) override;
    void on_timer   (const TimerSP&) override;
    void on_eof     () override;

    void drop_connection ();
    void analyze_request ();
    void finish_request  (const std::error_code&);
};

}}}
