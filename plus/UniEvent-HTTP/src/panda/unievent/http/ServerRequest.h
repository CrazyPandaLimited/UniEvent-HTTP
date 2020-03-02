#pragma once
#include "msg.h"
#include "ServerResponse.h"
#include <panda/CallbackDispatcher.h>

namespace panda { namespace unievent { namespace http {

struct Server;        using ServerSP        = iptr<Server>;
struct ServerRequest; using ServerRequestSP = iptr<ServerRequest>;
struct ServerConnection;

struct ServerRequest : protocol::http::Request {
    using receive_fptr = void(const ServerRequestSP&);
    using partial_fptr = void(const ServerRequestSP&, const std::error_code&);
    using drop_fptr    = void(const ServerRequestSP&, const std::error_code&);
    using receive_fn   = function<receive_fptr>;
    using partial_fn   = function<partial_fptr>;
    using drop_fn      = function<drop_fptr>;

    CallbackDispatcher<receive_fptr> receive_event;
    CallbackDispatcher<partial_fptr> partial_event;
    CallbackDispatcher<drop_fptr>    drop_event;

    bool is_done () { return _is_done; }

    const ServerResponseSP& response () const { return _response; }

    void enable_partial () { _partial = true; }

    virtual void respond (const ServerResponseSP&);
    virtual void send_continue ();

    virtual void redirect (const string&);
    void redirect (const URISP& uri) { redirect(uri->to_string()); }

    virtual void drop ();

protected:
    ServerRequest (ServerConnection* conn) : _connection(conn) {}

    ~ServerRequest ();

private:
    friend ServerConnection; friend ServerResponse;

    ServerConnection* _connection;
    ServerSP          _server;           // holds server while active
    ServerResponseSP  _response;
    bool              _routed            = false;
    bool              _partial           = false;
    bool              _finish_on_receive = false;
    bool              _is_done           = false;
};

}}}
