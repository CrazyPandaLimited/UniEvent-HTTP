#pragma once
#include "BasicRequest.h"
#include "ServerResponse.h"

namespace panda { namespace unievent { namespace http {

struct ServerConnection;

struct ServerRequest : BasicRequest {

    State                   state    () const { return _state; }
    const ServerResponseSP& response () const { return _response; }

    void respond (const ServerResponseSP&);

protected:
    ServerRequest (ServerConnection* conn) : _connection(conn), _partial_called(), _state(State::not_yet) {}

private:
    friend ServerConnection; friend ServerResponse;

    ServerConnection* _connection;
    ServerResponseSP  _response;
    bool              _partial_called;
    State             _state;
};
using ServerRequestSP = iptr<ServerRequest>;

}}}
