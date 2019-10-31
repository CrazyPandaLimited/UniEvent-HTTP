#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-keep-alive: " name, "[server-keep-alive]" VSSL)

TEST("server closes") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
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
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
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
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200, Header().connection("keep-alive")));
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
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200, Header().connection("close")));
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
