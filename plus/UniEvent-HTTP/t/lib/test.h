#pragma once
#include <catch.hpp>
#include <panda/unievent/http.h>
#include <panda/unievent/http/Server.h>
#include <panda/unievent/test/AsyncTest.h>

using namespace panda;
using panda::unievent::Tcp;
using panda::unievent::Loop;
using panda::unievent::TcpSP;
using panda::unievent::LoopSP;
using panda::unievent::Timer;
using panda::unievent::TimerSP;
using namespace panda::unievent::test;
using namespace panda::unievent::http;
using RawRequest    = panda::protocol::http::Request;
using RawRequestSP  = panda::protocol::http::RequestSP;
using RawResponseSP = panda::protocol::http::ResponseSP;

#define VSSL "[v-ssl]"

extern bool secure;

static auto fail_cb = [](auto...){ FAIL(); };

struct TServer : Server {
    using Server::Server;

    void   enable_echo ();
    void   autorespond (const ServerResponseSP&);
    string location    () const;

private:
    using Responses = std::deque<ServerResponseSP>;

    Responses autoresponse_queue;
    bool      autores = false;
};
using TServerSP = iptr<TServer>;

struct TClient : Client {
    using Client::Client;

    net::SockAddr sa;

    void request (const RequestSP&);

    ResponseSP get_response (const RequestSP& req);
    ResponseSP get_response (const string& uri, Header&& = {}, Body&& = {}, bool chunked = false);

    std::error_code get_error (const RequestSP& req);

    ~TClient () { panda_log_debug("~"); }
};
using TClientSP = iptr<TClient>;


struct ClientPair {
    TServerSP server;
    TClientSP client;

    ClientPair (const LoopSP&);
};


struct ServerPair {
    using Parser       = panda::protocol::http::ResponseParser;
    using RawResponses = std::deque<RawResponseSP>;

    TServerSP server;
    TcpSP     conn;

    ServerPair (const LoopSP&, Server::Config = {});

    RawResponseSP get_response ();
    RawResponseSP get_response (const string& s) { conn->write(s); return get_response(); }
    bool          wait_eof     (int tmt = 0);

private:
    Parser       parser;
    RawResponses response_queue;
    bool         eof = false;
};


TServerSP make_server (const LoopSP&, Server::Config = {});
