#pragma once

#include <map>
#include <vector>
#include <ctime>

#include <panda/refcnt.h>
#include <panda/CallbackDispatcher.h>
#include <panda/unievent/Timer.h>
#include <panda/unievent/Loop.h>
#include <panda/unievent/Stream.h>
#include <panda/time.h>

#include "../common/Defines.h"
#include "../common/Error.h"

#include "Listener.h"
#include "Connection.h"

//#define ENABLE_DUMP_SERVER_MESSAGES 1

namespace panda { namespace unievent { namespace http { namespace server {

class Server : public virtual Refcnt {
public:
    struct Config {
        std::vector<Location> locations;
        string                dump_file_prefix;
    };

    Server(Loop* loop = Loop::default_loop());

    void configure(const Config& config);

    Loop* loop() const { return _loop; }

    virtual void run();
    virtual void stop();

    void remove_connection(ConnectionSP conn);

    // gets cached http date string, keeps us from calling strftime every now and again
    const string& http_date_now() const;

    CallbackDispatcher<void(ServerSP, ConnectionSP)> connection_callback;
    CallbackDispatcher<void(ServerSP, ConnectionSP)> remove_connection_callback;
    CallbackDispatcher<void(ConnectionSP, protocol::http::RequestSP, ResponseSP&)> request_callback;

protected:
    // restrict stack allocation
    virtual ~Server();

    virtual void config_validate(const Config&) const;
    virtual void config_apply(const Config&);

    virtual ConnectionSP new_connection(uint64_t id);

    virtual void on_connection(ConnectionSP conn);
    virtual void on_remove_connection(ConnectionSP conn);

    void start_listening();
    void stop_listening();

    ConnectionSP get_connection(uint64_t id) {
        auto pos = connections.find(id);
        if(pos == connections.end()) {
            return nullptr;
        } else {
            return pos->second;
        }
    }

private:
    // disable copying as we disabled stack usage
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void on_connect(Stream* serv, Stream* handle, const CodeError* err);
    void on_disconnect(Stream* handle);

public:
    bool running;
    std::vector<Location>   locations;
    std::vector<ListenerSP> listeners;

    using ConnectionMap = std::map<uint64_t, ConnectionSP>;
    ConnectionMap connections;

private:
    static std::atomic<uint64_t> lastid;

    iptr<Loop> _loop;

    // time() and strftime() cache updater
    iptr<Timer> _cached_date_timer;

    static time_t cached_time;

    static thread_local string  cached_time_string_rfc;
    static thread_local string* cached_time_string_rfc_ptr;
};

template <typename Stream>
Stream& operator<<(Stream& stream, const Server::Config& conf) {
    stream << "ServerConfig{ locations:[";
    for (auto loc : conf.locations) {
        stream << loc << ",";
    }
    stream << "]};";
    return stream;
}

}}}}
