#pragma once
#include <catch.hpp>
#include <panda/log.h>
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
    static int dcnt;

    using Server::Server;

    void   enable_echo ();
    void   autorespond (const ServerResponseSP&);
    string location    () const;
    NetLoc netloc      () const;

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

    ~TClient () { ++dcnt; }

    friend struct TPool;
};
using TClientSP = iptr<TClient>;

struct TPool : Pool {
    using Pool::Pool;

    TClientSP get (const NetLoc& nl)                  { return dynamic_pointer_cast<TClient>(Pool::get(nl)); }
    TClientSP get (const string& host, uint16_t port) { return dynamic_pointer_cast<TClient>(Pool::get(host, port)); }

protected:
    ClientSP new_client () override { return new TClient(this); }
};
using TPoolSP = iptr<TPool>;


struct ClientPair {
    TServerSP server;
    TClientSP client;

    ClientPair (const LoopSP&);
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
    bool          wait_eof     (int tmt = 0);

private:
    Parser       parser;
    RawResponses response_queue;
    bool         eof = false;
};


TServerSP make_server (const LoopSP&, Server::Config = {});
