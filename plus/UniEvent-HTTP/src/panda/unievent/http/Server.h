#pragma once
#include "Error.h"
#include "ServerConnection.h"
#include <map>
#include <iosfwd>
#include <vector>
#include <atomic>
//#include <ctime>

//#include <panda/refcnt.h>
//#include <panda/CallbackDispatcher.h>
//#include <panda/unievent/Timer.h>
//#include <panda/unievent/Loop.h>
//#include <panda/unievent/Stream.h>
//#include <panda/time.h>

struct ssl_ctx_st; typedef ssl_ctx_st SSL_CTX; // avoid including openssl headers

namespace panda { namespace unievent { namespace http {

struct Server : Refcnt, private IStreamSelfListener {
    static constexpr const int DEFAULT_BACKLOG = 4096;

    struct Location {
        string   host;
        uint16_t port       = 0;
        bool     reuse_port = true; // several listeners(servers) can be bound to the same port if true, useful for threaded apps
        int      backlog    = DEFAULT_BACKLOG; // max accept queue
        SSL_CTX* ssl_ctx    = nullptr; // if set, will use SSL
    };

    struct Config {
        std::vector<Location> locations;
    };

    using request_fptr = void(const RequestSP&, const ServerConnectionSP&);
    using partial_fptr = void(const RequestSP&, Request::State, const std::error_code&, const ServerConnectionSP&);
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
    Connections _connections;
    //string      _cached_date;
    //TimerSP     _cached_date_timer;

    void start_listening ();
    void stop_listening  ();

    StreamSP create_connection (const StreamSP&) override;

    void on_connection (const StreamSP&, const StreamSP&, const CodeError&) override;

    void handle_partial (const RequestSP& req, Request::State state, const std::error_code& err, const ServerConnectionSP& conn) {
        partial_event(req, state, err, conn);
    }

    void handle_request (const RequestSP& req, const std::error_code& err, const ServerConnectionSP& conn) {
        request_event(req, err, conn);
    }

    // disable copying
    Server (const Server&) = delete;
    Server& operator= (const Server&) = delete;
    ~Server (); // restrict stack allocation

//    void remove_connection(ConnectionSP conn);
//
//    // gets cached http date string, keeps us from calling strftime every now and again
//    const string& http_date_now() const;
//
//    CallbackDispatcher<void(ServerSP, ConnectionSP)> connection_callback;
//    CallbackDispatcher<void(ServerSP, ConnectionSP)> remove_connection_callback;
//
//
//    virtual void on_connection(ConnectionSP conn);
//    virtual void on_remove_connection(ConnectionSP conn);
//
//    ConnectionSP get_connection(uint64_t id) {
//        auto pos = connections.find(id);
//        if(pos == connections.end()) {
//            return nullptr;
//        } else {
//            return pos->second;
//        }
//    }
//
//private:
//
//    void on_connect(const StreamSP&, const StreamSP&, const CodeError&);
//    void on_disconnect(Stream* handle);
//
//private:
//
//    // time() and strftime() cache updater
//    iptr<Timer> _cached_date_timer;
//
//    static time_t cached_time;
//
//    static thread_local string  cached_time_string_rfc;
//    static thread_local string* cached_time_string_rfc_ptr;
};

std::ostream& operator<< (std::ostream&, const Server::Location&);
std::ostream& operator<< (std::ostream&, const Server::Config&);

}}}
