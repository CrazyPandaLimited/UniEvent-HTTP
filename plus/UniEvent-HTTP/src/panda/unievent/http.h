#pragma once
#include "http/Pool.h"
#include "http/Request.h"
#include <panda/expected.h>

namespace panda { namespace unievent { namespace http {

struct sync_t {};

inline void http_request (const RequestSP& req, const LoopSP& loop = Loop::default_loop()) {
    Pool::instance(loop)->request(req);
}

expected<ResponseSP, std::error_code> http_request (const RequestSP& req, sync_t);

expected<string, std::error_code> http_get (const URISP& uri);

inline expected<string, std::error_code> http_get (const string& url) { return http_get(new URI(url)); }

}}}
