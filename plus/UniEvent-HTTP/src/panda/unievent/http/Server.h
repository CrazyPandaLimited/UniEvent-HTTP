#pragma once
#include "Error.h"
#include "ServerConnection.h"
#include <map>
#include <iosfwd>
#include <vector>
#include <atomic>

struct ssl_ctx_st; typedef ssl_ctx_st SSL_CTX; // avoid including openssl headers

namespace panda { namespace unievent { namespace http {

using panda::protocol::http::SIZE_UNLIMITED;

struct Server : Refcnt, private IStreamSelfListener {
    static constexpr const int      DEFAULT_BACKLOG          = 4096;
    static constexpr const uint32_t DEFAULT_IDLE_TIMEOUT     = 300000; // [ms]
    static constexpr const size_t   DEFAULT_MAX_HEADERS_SIZE = 16384;
    static constexpr const size_t   DEFAULT_MAX_BODY_SIZE    = SIZE_UNLIMITED;

    struct Location {
        string   host;
        uint16_t port       = 0;
        bool     reuse_port = true; // several listeners(servers) can be bound to the same port if true, useful for threaded apps
        int      backlog    = DEFAULT_BACKLOG; // max accept queue
        SSL_CTX* ssl_ctx    = nullptr; // if set, will use SSL
        int      domain     = AF_INET;
    };

    struct Config {
        std::vector<Location> locations;
        uint32_t              idle_timeout     = DEFAULT_IDLE_TIMEOUT;     // max idle time connection before it is dropped [ms], 0 = unlimited
        size_t                max_headers_size = DEFAULT_MAX_HEADERS_SIZE; // max size from the start of request to end of headers, 0 = unlimited
        size_t                max_body_size    = DEFAULT_MAX_BODY_SIZE;    // 0 = unlimited
    };

    using Listeners    = std::vector<TcpSP>;
    using route_fptr   = void(const ServerRequestSP&);
    using receive_fptr = ServerRequest::receive_fptr;
    using error_fptr   = void(const ServerRequestSP&, const std::error_code&);

    CallbackDispatcher<route_fptr>   route_event;
    CallbackDispatcher<receive_fptr> request_event;
    CallbackDispatcher<error_fptr>   error_event;

    Server (const LoopSP& loop = Loop::default_loop());

    void configure (const Config& config);

    const LoopSP&    loop      () const { return _loop; }
    const Listeners& listeners () const { return _listeners; }

    bool running () const { return _running; }

    virtual void run  ();
    virtual void stop ();

    const string& date_header_now ();

protected:
    virtual ServerConnectionSP new_connection (uint64_t id, const ServerConnection::Config&);

private:
    friend ServerConnection;
    using Locations   = std::vector<Location>;
    using Connections = std::map<uint64_t, ServerConnectionSP>;

    static std::atomic<uint64_t> lastid;

    LoopSP      _loop;
    Config      _conf;
    Listeners   _listeners;
    bool        _running;
    Connections _connections;
    uint64_t    _hdate_time;
    string      _hdate_str;

    void start_listening ();
    void stop_listening  ();

    StreamSP create_connection (const StreamSP&) override;

    void on_connection (const StreamSP&, const CodeError&) override;

    void remove (const ServerConnectionSP& conn) {
        _connections.erase(conn->id());
    }

    // disable copying
    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;
    ~Server (); // restrict stack allocation
};
using ServerSP = iptr<Server>;

std::ostream& operator<< (std::ostream&, const Server::Location&);
std::ostream& operator<< (std::ostream&, const Server::Config&);

}}}
