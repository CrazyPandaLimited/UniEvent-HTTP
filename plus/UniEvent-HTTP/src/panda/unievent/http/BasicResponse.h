#pragma once
#include <panda/protocol/http/Response.h>

namespace panda { namespace unievent { namespace http {

using protocol::http::Body;
using protocol::http::State;
using protocol::http::Header;
using protocol::http::HttpVersion;

struct BasicResponse : protocol::http::Response {
    using Response::Response;
};

}}}
