#pragma once

#include <sstream>

#include <panda/refcnt.h>
#include <panda/unievent/Fwd.h>
#include <panda/uri/URI.h>

namespace panda { namespace unievent { namespace http {

using URISP = iptr<uri::URI>;

class Response;
using ResponseSP = iptr<Response>;

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

//    class Response;
//    using ResponseSP = iptr<Response>;

    using ResponseCallback = function<void(RequestSP, ResponseSP)>;
    using RedirectCallback = function<void(RequestSP, URISP)>;
    using ErrorCallback = function<void(RequestSP, const string&)>;
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

//    class Request;
//    using RequestSP = iptr<Request>;

//    class Response;
//    using ResponseSP = iptr<Response>;
}

void http_request(client::RequestSP, client::ConnectionSP);
client::ConnectionSP http_request(client::RequestSP, client::ClientConnectionPool*);

template <class... Args> std::string collect(Args&&... args) {
    std::ostringstream ss;
    using expand = int[];
    void(expand{0, ((ss << args), 0)...});
    return ss.str();
}

}}} // namespace panda::unievent::http
