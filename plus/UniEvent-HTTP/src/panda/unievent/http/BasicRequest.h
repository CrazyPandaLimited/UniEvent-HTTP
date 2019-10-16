#pragma once
#include "BasicResponse.h"
#include <panda/protocol/http/Request.h>

namespace panda { namespace unievent { namespace http {

using panda::uri::URI;
using panda::uri::URISP;

struct BasicRequest : protocol::http::Request {
    using Method = protocol::http::Request::Method;
    using Request::Request;
};

}}}
