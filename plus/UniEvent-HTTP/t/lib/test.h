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

struct ServerPair {
    using Parser       = panda::protocol::http::ResponseParser;
    using Responses    = std::deque<ServerResponseSP>;
    using RawResponses = std::deque<RawResponseSP>;

    ServerSP   server;
    TcpSP      conn;

    ServerPair () : autores(), eof() {}

    RawResponseSP get_response ();
    RawResponseSP get_response (const string& s) { conn->write(s); return get_response(); }
    void          autorespond  (const ServerResponseSP&);
    bool          wait_eof     (int tmt = 0);

private:
    Parser       parser;
    Responses    autoresponse_queue;
    RawResponses response_queue;
    bool         autores;
    bool         eof;
};

ServerPair make_server_pair (const LoopSP&, Server::Config = {});

//#include <algorithm>
//#include <cstring>
//#include <exception>
//#include <iostream>
//#include <random>

//template<class R>
//R random_string_generator(size_t length, const char* alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") {
//    R result;
//    result.resize(length);
//
//    std::random_device rd;
//    std::mt19937 engine(rd());
//    std::uniform_int_distribution<> distribution(0, strlen(alphabet) - 1);
//    for(size_t i = 0; i < length; ++i) {
//       	result[i] = alphabet[distribution(engine)];
//    }
//    return result;
//}
//
//template<typename F>
//iptr<Timer> once(uint64_t timeout, iptr<Loop> loop, F&& f) {
//    iptr<Timer> timer = new Timer(loop);
//    timer->once(timeout);
//    timer->event.add([=](auto) {
//        timer->stop();
//        f();
//    });
//
//    return timer;
//}
//
//inline void run(Loop* loop = Loop::default_loop()) {
//    try {
//      loop->run();
//    } catch(Error& e) {
//        FAIL(e.what());
//    } catch (std::exception& e) {
//        FAIL(e.what());
//    }
//}
//
//inline void wait(uint64_t timeout, panda::iptr<Loop> loop) {
//    panda::iptr<Timer> timer = new Timer(loop);
//    timer->once(timeout);
//    timer->event.add([=](auto) {
//        timer->stop();
//        loop->stop();
//    });
//    run(loop);
//}
//
//inline iptr<Timer> add_timeout(iptr<Loop> loop, uint64_t timeout) {
//    return once(timeout, loop, [=]() {
//        loop->stop();
//        FAIL("timeout");
//    });
//}
//
//panda::iptr<protocol::http::Request> parse_request(const string& buf);
//
//// Send in request end get it back in reponse body
//std::tuple<protocol::http::ResponseSP, protocol::http::RequestSP> echo_request(
//        iptr<uri::URI> uri,
//        protocol::http::Request::Method request_method,
//        const string& body = "",
//        protocol::http::Header header = {});
//
//std::tuple<server::ServerSP, uint16_t> echo_server(const string& name);
//
//std::tuple<server::ServerSP, uint16_t> redirect_server(const string& name, uint16_t to_port);
