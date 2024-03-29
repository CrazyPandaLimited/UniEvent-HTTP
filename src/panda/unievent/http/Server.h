#pragma once
#include "error.h"
#include "ServerConnection.h"
#include <map>
#include <iosfwd>
#include <vector>
#include <atomic>

namespace panda { namespace unievent { namespace http {

using panda::protocol::http::SIZE_UNLIMITED;

struct Server : Refcnt, private IStreamListener {
    static constexpr const int      DEFAULT_BACKLOG          = 4096;
    static constexpr const uint32_t DEFAULT_IDLE_TIMEOUT     = 300000; // [ms]
    static constexpr const size_t   DEFAULT_MAX_HEADERS_SIZE = 16384;
    static constexpr const size_t   DEFAULT_MAX_BODY_SIZE    = SIZE_UNLIMITED;

    #ifdef _WIN32
    static constexpr const bool DEFRPORT = false;
    #else
    static constexpr const bool DEFRPORT = true;
    #endif

    struct Location {
        string           host;
        uint16_t         port        = 0;
        string           path        = {};              // path for unix sockets or name for named pipes (windows), if supplied ignores host, port, domain, reuse_port
        bool             reuse_port  = DEFRPORT;        // several listeners(servers) can be bound to the same port if true, useful for threaded apps
        int              backlog     = DEFAULT_BACKLOG; // max accept queue
        int              domain      = AF_INET;
        bool             tcp_nodelay = false;           // if the location is a tcp location, enables tcp nodelay feature
        SslContext       ssl_ctx     = nullptr;         // if set, will use SSL
        optional<sock_t> sock        = {};              // if supplied, uses this socket and ignores host, port, path, reuse_port, backlog, domain
                                                        // socket must be bound but NOT LISTENING!

        Location () {}

        Location (const string& host, uint16_t port, int backlog = DEFAULT_BACKLOG, const SslContext& ssl_ctx = {})
            : host(host), port(port), backlog(backlog), ssl_ctx(ssl_ctx) {}

        Location& set_host       (const string& val)     { host = val; return *this; }
        Location& set_port       (uint16_t val)          { port = val; return *this; }
        Location& set_path       (const string& val)     { path = val; return *this; }
        Location& set_reuse_port (bool val)              { reuse_port = val; return *this; }
        Location& set_backlog    (int val)               { backlog = val; return *this; }
        Location& set_domain     (int val)               { domain = val; return *this; }
        Location& set_ssl_ctx    (const SslContext& val) { ssl_ctx = val; return *this; }
        Location& set_sock       (sock_t val)            { sock = val; return *this; }
        Location& set_tcp_nodelay(bool val)              { tcp_nodelay = val; return *this; }

        bool operator== (const Location&) const;
        bool operator!= (const Location& oth) const { return !operator==(oth); }
    };

    struct Config {
        using Locations = std::vector<Location>;
        Locations locations;
        uint32_t  idle_timeout           = DEFAULT_IDLE_TIMEOUT;     // max idle time connection before it is dropped [ms], 0 = unlimited
        size_t    max_headers_size       = DEFAULT_MAX_HEADERS_SIZE; // max size from the start of request to end of headers, 0 = unlimited
        size_t    max_body_size          = DEFAULT_MAX_BODY_SIZE;    // 0 = unlimited
        bool      tcp_nodelay            = false;
        uint32_t  max_keepalive_requests = 0;                        // respond with "connection: close" in KA connection after that number of requests (0 = unlimited)

        bool operator== (const Config&) const;
        bool operator!= (const Config& oth) const { return !operator==(oth); }
    };

    using Listeners    = std::vector<StreamSP>;
    using run_fptr     = void();
    using route_fptr   = void(const ServerRequestSP&);
    using request_fptr = ServerRequest::receive_fptr;
    using error_fptr   = void(const ServerRequestSP&, const ErrorCode&);
    using stop_fptr    = void();
    using connect_fptr = void(const ServerConnectionSP&);

    using run_fn       = function<run_fptr>;
    using route_fn     = function<route_fptr>;
    using request_fn   = ServerRequest::receive_fn;
    using error_fn     = function<error_fptr>;
    using stop_fn      = function<stop_fptr>;
    using connect_fn   = function<connect_fptr>;
    using IFactory     = ServerConnection::IFactory;

    CallbackDispatcher<run_fptr>     run_event;
    CallbackDispatcher<route_fptr>   route_event;
    CallbackDispatcher<request_fptr> request_event;
    CallbackDispatcher<error_fptr>   error_event;
    CallbackDispatcher<stop_fptr>    stop_event;
    CallbackDispatcher<connect_fptr> connect_event;

    Server (const LoopSP& loop = Loop::default_loop(), IFactory* = nullptr);
    Server (const Config& config, const LoopSP& loop = Loop::default_loop(), IFactory* = nullptr);

    virtual void configure (const Config& config);

    const LoopSP&    loop      () const { return _loop; }
    const Listeners& listeners () const { return _listeners; }

    bool running  () const { return _state == State::running; }
    bool stopping () const { return _state == State::stopping; }

    virtual void run  ();
    virtual void stop ();
    virtual void graceful_stop ();

    virtual void start_listening ();
    virtual void stop_listening  ();

    excepted<net::SockAddr, ErrorCode> sockaddr () const;

    const string& date_header_now ();

protected:
    virtual ServerConnectionSP new_connection (uint64_t id, const ServerConnection::Config&, const StreamSP&);

    ~Server (); // restrict stack allocation

private:
    enum class State { initial, running, stopping };
    friend ServerConnection;
    using Locations   = std::vector<Location>;
    using Connections = std::map<uint64_t, ServerConnectionSP>;

    static std::atomic<uint64_t> lastid;

    LoopSP      _loop;
    IFactory*   _factory = nullptr;
    Config      _conf;
    Listeners   _listeners;
    State       _state = State::initial;
    Connections _connections;
    uint64_t    _hdate_time = 0;
    string      _hdate_str;

    void on_establish(const StreamSP&, const StreamSP&, const ErrorCode&) override;

    void remove (const ServerConnectionSP& conn) {
        _connections.erase(conn->id());
        if (_state == State::stopping) _stop_if_done();
    }

    void _stop_if_done () {
        assert(_state == State::stopping);
        if (_connections.size()) return;
        _state = State::initial;
        stop_event();
    }

    // disable copying
    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;
};
using ServerSP = iptr<Server>;

std::ostream& operator<< (std::ostream&, const Server::Location&);
std::ostream& operator<< (std::ostream&, const Server::Config&);

}}}
