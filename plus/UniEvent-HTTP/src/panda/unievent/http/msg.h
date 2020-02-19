#pragma once
#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/log.h>

namespace panda { namespace unievent { namespace http {

using protocol::http::Body;
using protocol::http::State;
using protocol::http::Headers;

using panda::uri::URI;
using panda::uri::URISP;

extern log::Module uewslog;

}}}
