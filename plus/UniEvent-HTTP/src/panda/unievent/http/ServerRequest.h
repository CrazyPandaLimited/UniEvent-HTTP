#pragma once
#include "msg.h"
#include "ServerResponse.h"
#include <panda/CallbackDispatcher.h>

namespace panda { namespace unievent { namespace http {

struct ServerRequest;
using ServerRequestSP = iptr<ServerRequest>;
struct ServerConnection;

struct ServerRequest : protocol::http::Request {
    using receive_fptr = void(const ServerRequestSP&);
    using partial_fptr = void(const ServerRequestSP&, const std::error_code&);
    using drop_fptr    = void(const ServerRequestSP&, const std::error_code&);

    CallbackDispatcher<receive_fptr> receive_event;
    CallbackDispatcher<partial_fptr> partial_event;
    CallbackDispatcher<drop_fptr>    drop_event;

    State                   state    () const { return _state; }
    const ServerResponseSP& response () const { return _response; }

    void enable_partial () { _partial = true; }

    void respond (const ServerResponseSP&);

protected:
    ServerRequest (ServerConnection* conn) : _connection(conn), _state(State::not_yet), _routed(), _partial(), _finish_on_receive() {}

    ~ServerRequest () {
        // remove garbage from response in case if user holds response without request after response is finished
        if (_response) _response->_request = nullptr;
    }

private:
    friend ServerConnection; friend ServerResponse;

    ServerConnection* _connection;
    ServerResponseSP  _response;
    State             _state;
    bool              _routed;
    bool              _partial;
    bool              _finish_on_receive;
};

}}}
