#pragma once

#include <catch.hpp>

#ifdef seed
    #undef seed
    #include <algorithm>
    #include <cstring>
    #include <exception>
    #include <iostream>
    #include <random>
    #define seed() Perl_seed(aTHX)
#else
    #include <algorithm>
    #include <cstring>
    #include <exception>
    #include <iostream>
    #include <random>
#endif

#include <panda/unievent.h>
#include <panda/unievent/test/AsyncTest.h>
#include <panda/protocol/http/Request.h>
#include <panda/protocol/http/Response.h>
#include <panda/protocol/http/ResponseParser.h>
#include <panda/unievent/http/client/Request.h>
#include <panda/unievent/http/server/Server.h>
#include <panda/unievent/http/server/Location.h>

using namespace panda;
using namespace unievent;
using namespace http;
using namespace test;

inline bool check_internet_available() {
    return true;
    //FILE *output;
    //if(!(output = popen("/sbin/route -n | grep -c '^0\\.0\\.0\\.0'","r"))) {
    //    return false;
    //}
    //unsigned int i;
    //if(fscanf(output,"%u",&i) != 1) {
    //    return false;
    //}
    //
    //pclose(output);
    //return i == 1;
}

template<class R>
R random_string_generator(size_t length, const char* alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") {
    R result;
    result.resize(length);

    std::random_device rd;
    std::mt19937 engine(rd());
    std::uniform_int_distribution<> distribution(0, strlen(alphabet) - 1);
    for(size_t i = 0; i < length; ++i) {
       	result[i] = alphabet[distribution(engine)];
    }
    return result;
}

template<typename F>
iptr<Timer> once(uint64_t timeout, iptr<Loop> loop, F&& f) {
    iptr<Timer> timer = new Timer(loop);
    timer->once(timeout);
    timer->event.add([=](auto) {
        timer->stop();
        f();
    });

    return timer;
}

inline void run(Loop* loop = Loop::default_loop()) {
    try {
      loop->run();
    } catch(Error& e) {
        FAIL(e.what());
    } catch (std::exception& e) {
        FAIL(e.what());
    }
}

inline void wait(uint64_t timeout, panda::iptr<Loop> loop) {
    panda::iptr<Timer> timer = new Timer(loop);
    timer->once(timeout);
    timer->event.add([=](auto) {
        timer->stop();
        loop->stop();
    });
    run(loop);
}

inline iptr<Timer> add_timeout(iptr<Loop> loop, uint64_t timeout) {
    return once(timeout, loop, [=]() {
        loop->stop();
        FAIL("timeout");
    });
}

panda::iptr<protocol::http::Request> parse_request(const string& buf);

// Send in request end get it back in reponse body
std::tuple<protocol::http::ResponseSP, protocol::http::RequestSP> echo_request(
        iptr<uri::URI> uri,
        protocol::http::Request::Method request_method,
        const string& body = "",
        protocol::http::Header header = {});

std::tuple<server::ServerSP, uint16_t> echo_server(const string& name);

std::tuple<server::ServerSP, uint16_t> redirect_server(const string& name, uint16_t to_port);
