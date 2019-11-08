#pragma once
#include "msg.h"

namespace panda { namespace unievent { namespace http {

struct Response : protocol::http::Response {
    Response () {}

    State state () const { return _state; }

private:
    friend struct Client;

    State _state;
};
using ResponseSP = iptr<Response>;

}}}
