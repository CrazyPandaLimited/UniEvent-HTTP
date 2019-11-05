#pragma once
#include "msg.h"

namespace panda { namespace unievent { namespace http {

struct Response : protocol::http::Response {
    Response () {}
};
using ResponseSP = iptr<Response>;

}}}
