#include <panda/refcnt.h>
#include <panda/unievent/Loop.h>
#include <panda/unievent/Timer.h>

namespace panda { namespace unievent { namespace http { 

namespace client {
    using unievent::Loop;
    using unievent::Timer;
    using unievent::Stream;
    using unievent::TCP;
    using unievent::CodeError;
    using unievent::ConnectRequest;

    class Connection;
    using ConnectionSP = iptr<Connection>;

    template<class C> class ConnectionPool;

    using ClientConnectionPool = ConnectionPool<Connection>;

    class Request; 
    using RequestSP = iptr<Request>;

    class Response;
    using ResponseSP = iptr<Response>;
}

namespace server {
    using unievent::Loop;
    using unievent::Timer;
    using unievent::Stream;
    using unievent::TCP;
    using unievent::CodeError;
    using unievent::ConnectRequest;

    class Server;
    using ServerSP = iptr<Server>;

    class Connection; 
    using ConnectionSP = iptr<Connection>;

    struct Listener;
    using ListenerSP = iptr<Listener>;

    struct Location;
    using LocationSP = iptr<Location>;

    class Request; 
    using RequestSP = iptr<Request>;

    class Response;
    using ResponseSP = iptr<Response>;
}

}}} // namespace panda::unievent::http
