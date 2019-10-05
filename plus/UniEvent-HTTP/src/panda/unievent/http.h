#pragma once
#include "http/Pool.h"
#include "http/Request.h"

namespace panda { namespace unievent { namespace http {

inline void http_request (const RequestSP& req, const LoopSP& loop = Loop::default_loop()) {
    Pool::instance(loop)->request(req);
}

}}}
