#include "../lib/test.h"
#include <thread>

#define TEST(name) TEST_CASE("client-sync: " name, "[client-sync]" VSSL)

TEST("basic") {
    LoopSP sloop = new Loop();
    AsyncSP async = new Async(sloop);
    auto server = make_server(sloop);
    auto uri = server->uri();

    auto thr = std::thread([&] {
        async->event.add([&](auto) {
            sloop->stop();
        });

        server->request_event.add([](auto& req) {
            req->respond(new ServerResponse(200, Headers(), Body("hello world")));
        });
        sloop->run();

        server->stop();
    });

    auto result = http_request(Request::Builder().uri(uri).timeout(10000).build(), sync_t());
    async->send();
    thr.join();

    REQUIRE(result);
    auto res = result.value();
    REQUIRE(res);
    CHECK(res->body.to_string() == "hello world");
}

TEST("simple get") {
    LoopSP sloop = new Loop();
    AsyncSP async = new Async(sloop);
    auto server = make_server(sloop);
    auto uri = server->uri();

    auto thr = std::thread([&] {
        async->event.add([&](auto) {
            sloop->stop();
        });

        server->request_event.add([](auto& req) {
            req->respond(new ServerResponse(200, Headers(), Body("big file")));
        });
        sloop->run();

        server->stop();
    });

    auto txt = http_get(uri).value();
    async->send();
    thr.join();

    CHECK(txt == "big file");
}
