#pragma once
//#include <deque>
#include "Request.h"
#include <panda/unievent/Tcp.h>
#include <panda/protocol/http/RequestParser.h>

namespace panda { namespace unievent { namespace http {

struct Server;

struct ServerConnection : Tcp, private ITcpSelfListener, private protocol::http::IRequestFactory {
    ServerConnection (Server*, uint64_t id);

    ~ServerConnection ();

    uint64_t id () const { return _id; }

    void respond (const RequestSP&, const ResponseSP&);

//    virtual void close(uint16_t code, string payload);
//
//    Server* server() const { return server_; }
//
//    CallbackDispatcher<void(ConnectionSP, protocol::http::RequestSP, ResponseSP&)> request_callback;
//    CallbackDispatcher<void(ConnectionSP, const CodeError&)> stream_error_callback;
//    CallbackDispatcher<void(ConnectionSP, const string&)> any_error_callback;
//    CallbackDispatcher<void(ConnectionSP, uint16_t, string)> close_callback;

private:

//    virtual void on_request(protocol::http::RequestSP msg);
//    virtual void on_stream_error(const CodeError& err);
//    virtual void on_any_error(const string& err);
//
//    virtual void on_eof() override;
//
//    void close_tcp();
//
//
//    void write_chunk(const string& buf, bool is_last);

private:
    using RequestParser = protocol::http::RequestParser;

    struct RequestData {
        RequestSP      request;
        ResponseSP     response;
        bool           partial_called;
        Request::State state;
        RequestData (const RequestSP& req) : request(req), partial_called(), responded(), state(Request::State::not_yet) {}
    };
    using RequestList = std::deque<RequestData>;

    Server*       _server;
    uint64_t      _id;
    RequestParser _parser;
    RequestList   _requests;

    protocol::http::RequestSP create_request () { return new Request(this); }

    void on_read (string& buf, const CodeError& err) override;

    void request_error (RequestData&, const std::error_code& err);

    RequestData* find_data (const RequestSP& req) {
        for (auto& rd : _requests) if (rd.request == req) return &rd;
        return nullptr;
    }

    bool responded (const RequestSP& req) {
        auto rd = find_data(req);
        return !rd || rd.response;
    }
};
using ServerConnectionSP = iptr<ServerConnection>;

}}}
