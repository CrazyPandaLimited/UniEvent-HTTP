#pragma once
#include <catch.hpp>
#include <panda/log.h>
#include <panda/unievent/http.h>
#include <panda/unievent/http/Server.h>
#include <panda/unievent/test/AsyncTest.h>
#include <memory>
#include <functional>

using namespace panda;
using panda::unievent::Tcp;
using panda::unievent::TcpSP;
using panda::unievent::Loop;
using panda::unievent::LoopSP;
using panda::unievent::Async;
using panda::unievent::AsyncSP;
using panda::unievent::Timer;
using panda::unievent::TimerSP;
using namespace panda::unievent::test;
using namespace panda::unievent::http;
using RawRequest    = panda::protocol::http::Request;
using RawRequestSP  = panda::protocol::http::RequestSP;
using RawResponseSP = panda::protocol::http::ResponseSP;

namespace compression = panda::protocol::http::compression;

#define VSSL "[v-ssl]"

extern bool secure;

string active_scheme();

static auto fail_cb = [](auto...){ FAIL(); };

using SslHolder = std::unique_ptr<SSL_CTX, std::function<void(SSL_CTX*)>>;

int64_t get_time     ();
void    time_mark    ();
int64_t time_elapsed ();

struct TServer : Server {
    static int dcnt;

    using Server::Server;

    void   enable_echo ();
    void   autorespond (const ServerResponseSP&);
    string location    () const;
    NetLoc netloc      () const;
    string uri         () const;

    static SslHolder get_context(string cert_name = "ca");

    ~TServer () { ++dcnt; }

private:
    using Responses = std::deque<ServerResponseSP>;

    Responses autoresponse_queue;
    bool      autores = false;
};
using TServerSP = iptr<TServer>;

struct TClient : Client {
    static int dcnt;
    net::SockAddr sa;

    using Client::Client;

    void request (const RequestSP&);

    ResponseSP get_response (const RequestSP& req);
    ResponseSP get_response (const string& uri, Headers&& = {}, Body&& = {}, bool chunked = false);

    std::error_code get_error (const RequestSP& req);
    std::error_code get_error (const string& uri, Headers&& = {}, Body&& = {}, bool chunked = false);

    static SslHolder get_context(string cert_name, const string& ca_name = "ca");

    ~TClient () { ++dcnt; }

    friend struct TPool;
};
using TClientSP = iptr<TClient>;

struct TPool : Pool {
    using Pool::Pool;

    TClientSP request (const RequestSP& req);
    std::vector<ResponseSP> await_responses(std::vector<RequestSP>& reqs);

protected:
    ClientSP new_client () override { return new TClient(this); }
};
using TPoolSP = iptr<TPool>;

struct TProxy {
    TcpSP server;
    URISP url;
};

TProxy new_proxy(const LoopSP&, const net::SockAddr& sa = net::SockAddr::Inet4("127.0.0.1", 0));

struct ClientPair {
    TServerSP server;
    TClientSP client;
    TProxy proxy;

    ClientPair (const LoopSP&, bool with_proxy = false);
};


struct ServerPair {
    using Parser       = panda::protocol::http::ResponseParser;
    using RawResponses = std::deque<RawResponseSP>;

    TServerSP    server;
    TcpSP        conn;
    RawRequestSP source_request;

    ServerPair (const LoopSP&, Server::Config = {});

    RawResponseSP get_response ();
    RawResponseSP get_response (const string& s) { conn->write(s); return get_response(); }
    int64_t       wait_eof     (int tmt = 0);

private:
    Parser       parser;
    RawResponses response_queue;
    int64_t      eof = 0;
};


TServerSP make_server (const LoopSP&, Server::Config = {});
