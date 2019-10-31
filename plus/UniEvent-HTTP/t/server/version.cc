#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-version: " name, "[server-version]" VSSL)

TEST("preserves version 1.0") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
    auto res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_0);
}

TEST("preserves version 1.1") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
    auto res = p.get_response("GET / HTTP/1.1\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_1);
}

TEST("forces version 1.1 when chunks") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200, Header(), Body({"stsuka ", "nah"}), true));
    auto res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_1);
    CHECK(res->chunked);
    CHECK(res->body.to_string() == "stsuka nah");
    CHECK(res->body.parts.size() == 2);
}
