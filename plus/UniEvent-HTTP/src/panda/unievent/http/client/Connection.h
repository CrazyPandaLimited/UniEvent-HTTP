#pragma once

#include <cstdint>

#include <panda/refcnt.h>
#include <panda/string.h>
#include <panda/uri/URI.h>
#include <panda/unievent/Loop.h>
#include <panda/unievent/TCP.h>
#include <panda/unievent/Timer.h>
#include <panda/CallbackDispatcher.h>
#include <panda/protocol/http/ResponseParser.h>

#include "../common/Defines.h"
#include "../common/HostAndPort.h"

namespace panda { namespace unievent { namespace http { namespace client {

class Connection : public TCP {
    static constexpr uint64_t DEFAULT_CLOSE_TIMEOUT = 5000; // [ms]
public:
    Connection(const HostAndPort& host_and_port, Loop* loop, ClientConnectionPool* pool=nullptr, uint64_t close_timeout=DEFAULT_CLOSE_TIMEOUT);

    virtual ~Connection();

    CallbackDispatcher<void(Connection*, const CodeError*)> stream_error_callback;
    CallbackDispatcher<void(Connection*, const string&)> any_error_callback;
    CallbackDispatcher<void(Connection*)> close_callback;
    CallbackDispatcher<void(int)> connect_callback;

    void request(RequestSP request);

    void detach(RequestSP request);

    void connect(const string& host, unsigned short port);

    void close();

    bool connected() const {
        return connected_;
    }

    void on_connect(const CodeError* err, ConnectRequest* req) override;

    void on_read(string& buf, const CodeError* err) override;

    HostAndPort get_host_and_port() const { return host_and_port_; }

protected:
    virtual void on_response(RequestSP request, ResponseSP response);
    virtual void on_stream_error(const CodeError* err);
    virtual void on_any_error(const string& error_str);

    virtual void on_eof() override;

private:
    HostAndPort host_and_port_;
    ClientConnectionPool* pool_;
    uint64_t inactivity_timeout_;
    iptr<Timer> inactivity_timer_;
    RequestSP request_;
    protocol::http::ResponseParserSP response_parser_;
    bool connected_;
};

}}}} // namespace panda::unievent::http::client
