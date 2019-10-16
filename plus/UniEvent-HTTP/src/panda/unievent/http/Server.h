#pragma once
#include "Error.h"
#include "ServerConnection.h"
#include <map>
#include <iosfwd>
#include <vector>
#include <atomic>

struct ssl_ctx_st; typedef ssl_ctx_st SSL_CTX; // avoid including openssl headers

namespace panda { namespace unievent { namespace http {

struct Server : Refcnt, private IStreamSelfListener {
    static constexpr const int      DEFAULT_BACKLOG  = 4096;
    static constexpr const uint32_t DEFAULT_MAX_IDLE = 600; // [s]

    struct Location {
        string   host;
        uint16_t port       = 0;
        bool     reuse_port = true; // several listeners(servers) can be bound to the same port if true, useful for threaded apps
        int      backlog    = DEFAULT_BACKLOG; // max accept queue
        SSL_CTX* ssl_ctx    = nullptr; // if set, will use SSL
    };

    struct Config {
        std::vector<Location> locations;
        uint32_t              max_idle = DEFAULT_MAX_IDLE; // max idle time for keep-alive connection before it is dropped [s]
    };

    using request_fptr = void(const RequestSP&, const ServerConnectionSP&);
    using partial_fptr = void(const RequestSP&, const std::error_code&, const ServerConnectionSP&);
    using request_fn   = function<request_fptr>;
    using partial_fn   = function<partial_fptr>;

    CallbackDispatcher<request_fptr> request_event;
    CallbackDispatcher<partial_fptr> partial_event;

    Server (const LoopSP& loop = Loop::default_loop());

    void configure (const Config& config);

    const LoopSP& loop () const { return _loop; }

    bool running () const { return _running; }

    virtual void run  ();
    virtual void stop ();

    const string& date_header_now ();

protected:
    virtual ServerConnectionSP new_connection (uint64_t id);

private:
    friend ServerConnection;
    using Locations   = std::vector<Location>;
    using Listeners   = std::vector<TcpSP>;
    using Connections = std::map<uint64_t, ServerConnectionSP>;

    static std::atomic<uint64_t> lastid;

    LoopSP      _loop;
    Locations   _locations;
    Listeners   _listeners;
    bool        _running;
    uint32_t    _max_idle;
    Connections _connections;
    uint64_t    _hdate_time;
    string      _hdate_str;

    void start_listening ();
    void stop_listening  ();

    StreamSP create_connection (const StreamSP&) override;

    void on_connection (const StreamSP&, const CodeError&) override;

    void handle_partial (const RequestSP& req, const std::error_code& err, const ServerConnectionSP& conn) {
        partial_event(req, err, conn);
    }

    void handle_request (const RequestSP& req, const ServerConnectionSP& conn) {
        request_event(req, conn);
    }

    void handle_eof (const ServerConnectionSP& conn) {
        _connections.erase(conn->id());
    }

    // disable copying
    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;
    ~Server (); // restrict stack allocation
};

std::ostream& operator<< (std::ostream&, const Server::Location&);
std::ostream& operator<< (std::ostream&, const Server::Config&);

}}}
