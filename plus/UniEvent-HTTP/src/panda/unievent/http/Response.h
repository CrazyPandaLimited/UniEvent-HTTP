#pragma once
#include "BasicResponse.h"

namespace panda { namespace unievent { namespace http {

struct Response : BasicResponse {
    Response () {}
};
using ResponseSP = iptr<Response>;

}}}
