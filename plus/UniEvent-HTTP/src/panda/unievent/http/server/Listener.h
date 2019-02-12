#pragma once

#include <panda/refcnt.h>
#include <panda/string.h>
#include <panda/unievent/Loop.h>
#include <panda/unievent/TCP.h>

#include "../common/Defines.h"

#include "Location.h"

namespace panda { namespace unievent { namespace http { namespace server {

struct Listener : TCP {
    ~Listener() { _EDTOR(); }
    Listener(Loop* loop, Location& loc);
    void run();

    Location location;
};

}}}} // namespace panda::unievent::http::server
