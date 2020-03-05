#pragma once
#include "http/Pool.h"
#include "http/Request.h"
#include <panda/expected.h>

namespace panda { namespace unievent { namespace http {

struct sync_t {};

inline void http_request (const RequestSP& req, const LoopSP& loop = Loop::default_loop()) {
    Pool::instance(loop)->request(req);
}

expected<ResponseSP, ErrorCode> http_request (const RequestSP& req, sync_t);

expected<string, ErrorCode> http_get (const URISP& uri);

inline expected<string, ErrorCode> http_get (const string& url) { return http_get(new URI(url)); }

}}}
