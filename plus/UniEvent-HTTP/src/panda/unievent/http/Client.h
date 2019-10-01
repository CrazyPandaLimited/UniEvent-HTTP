#pragma once
#include "Request.h"
#include <panda/unievent/Tcp.h>
#include <panda/unievent/Timer.h>
#include <panda/protocol/http/ResponseParser.h>

namespace panda { namespace unievent { namespace http {

struct NetLoc {
    string   host;
    uint16_t port;

    bool operator== (const NetLoc& other) const { return host == other.host && port == other.port; }
};
std::ostream& operator<< (std::ostream& os, const NetLoc& h);

struct Pool;
struct Client;
using ClientSP = iptr<Client>;

struct Client : Tcp, private ITcpSelfListener, private ITimerSelfListener {
    Client (const LoopSP&);
    Client (Pool*);

    ~Client ();

    void request (const RequestSP&);
    void cancel  (const std::error_code& = std::errc::operation_canceled);

    //void detach(RequestSP request);

    //void connect(const string& host, unsigned short port);

    //void close();

    //bool connected() const {
    //    return connected_;
    //}


    //void on_read(string& buf, const CodeError& err) override;

    uint64_t      last_activity_time () const { return _last_activity_time; }
    const NetLoc& last_netloc        () const { return _netloc; }

protected:
//    virtual void on_response(RequestSP request, ResponseSP response);
//    virtual void on_stream_error(const CodeError& err);
//    virtual void on_any_error(const string& error_str);
//
//    virtual void on_eof() override;

private:
    using protocol::http::ResponseParser;

    Pool*          _pool;
    NetLoc         _netloc;
    RequestSP      _request;
    ResponseSP     _response;
    TimerSP        _timer;
    ResponseParser _response_parser;
    uint64_t       _last_activity_time;

    void on_connect (const CodeError&, const ConnectRequestSP&) override;
    void on_timer   () override;

    void handle_response ();
};

}}}
