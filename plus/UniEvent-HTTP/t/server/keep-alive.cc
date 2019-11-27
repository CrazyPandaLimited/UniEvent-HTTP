#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-keep-alive: " name, "[server-keep-alive]" VSSL)

TEST("server closes") {
    AsyncTest test(1000);
    ServerPair p(test.loop);
    p.server->autorespond(new ServerResponse(200));
    RawResponseSP res;

    SECTION("when 1.0 and no C") {
        res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    }
    SECTION("when 1.0 and C=CLOSE") {
        res = p.get_response("GET / HTTP/1.0\r\nConnection: close\r\n\r\n");
    }
    SECTION("when 1.1 and C=CLOSE") {
        res = p.get_response("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
    }

    p.wait_eof();
    CHECK(res->code == 200);
}

TEST("server persists") {
    AsyncTest test(1000);
    ServerPair p(test.loop);
    p.server->autorespond(new ServerResponse(200));
    RawResponseSP res;

    SECTION("when 1.0 and C=KA") {
        res = p.get_response("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    }
    SECTION("when 1.1 and no C") {
        res = p.get_response("GET / HTTP/1.1\r\n\r\n");
    }
    SECTION("when 1.1 and C=KA") {
        res = p.get_response("GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n");
    }

    CHECK(!p.wait_eof(5));
    CHECK(res->code == 200);
}

TEST("if req is <close>, then response also <close> regardless of user's choice in headers") {
    AsyncTest test(1000);
    ServerPair p(test.loop);
    p.server->autorespond(new ServerResponse(200, Headers().connection("keep-alive")));
    RawResponseSP res;

    SECTION("1.0 no C") {
        res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    }
    SECTION("1.1 and C=CLOSE") {
        res = p.get_response("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
    }

    p.wait_eof();
    CHECK(res->code == 200);
    CHECK(res->headers.connection() == "close");
}

TEST("if user's response says <close> then don't give a fuck what request says") {
    AsyncTest test(1000);
    ServerPair p(test.loop);
    p.server->autorespond(new ServerResponse(200, Headers().connection("close")));
    RawResponseSP res;

    SECTION("1.0 and C=KA") {
        res = p.get_response("GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n");
    }
    SECTION("1.1 and no C") {
        res = p.get_response("GET / HTTP/1.1\r\n\r\n");
    }
    SECTION("1.1 and C=KA") {
        res = p.get_response("GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n");
    }

    p.wait_eof();
    CHECK(res->code == 200);
    CHECK(res->headers.connection() == "close");
}

TEST("idle timeout before any requests") {
    AsyncTest test(1000);
    Server::Config cfg;
    cfg.idle_timeout = 10;
    ServerPair p(test.loop, cfg);
    CHECK(!p.wait_eof(5));
    CHECK(p.wait_eof(50));
}

TEST("idle timeout during and after request") {
    AsyncTest test(1000);
    Server::Config cfg;
    cfg.idle_timeout = 10;

    ServerPair p(test.loop, cfg);
    TimerSP t = new Timer(test.loop);

    p.server->request_event.add([&](auto& req){
        t->event.add([&, req](auto){
            req->respond(new ServerResponse(200, Headers(), Body("hello world")));
        });
        t->once(15); // longer that idle timeout, it should not break connection during active request
    });

    auto res = p.get_response("GET / HTTP/1.1\r\n\r\n");
    CHECK(res->body.to_string() == "hello world");

    CHECK(!p.wait_eof(5)); // after active request finishes we have 10ms again, so no disconnects for at least 5ms
    CHECK(p.wait_eof(50)); // and soon we are disconnected
}
