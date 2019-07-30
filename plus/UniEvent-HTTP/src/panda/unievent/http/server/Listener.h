#pragma once
#include "Location.h"
#include "../common/Defines.h"
#include <panda/unievent/Tcp.h>

namespace panda { namespace unievent { namespace http { namespace server {

struct Listener : Tcp {
    ~Listener() { _EDTOR(); }
    Listener(Loop* loop, Location& loc);
    void run();

    Location location;
};

}}}} // namespace panda::unievent::http::server
