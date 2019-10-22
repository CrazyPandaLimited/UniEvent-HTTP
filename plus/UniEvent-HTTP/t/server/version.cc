#include "../lib/test.h"

#define TGROUP "[server-version]"

TEST_CASE("server preserves version 1.0", TGROUP VSSL) {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
    auto res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_0);
}

TEST_CASE("server preserves version 1.1", TGROUP VSSL) {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200));
    auto res = p.get_response("GET / HTTP/1.1\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_1);
}

TEST_CASE("server forces version 1.1 when chunks", TGROUP VSSL) {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);
    p.autorespond(new ServerResponse(200, Header(), Body({"stsuka ", "nah"}), true));
    auto res = p.get_response("GET / HTTP/1.0\r\n\r\n");
    CHECK(res->http_version == HttpVersion::v1_1);
    CHECK(res->chunked);
    CHECK(res->body.to_string() == "stsuka nah");
    CHECK(res->body.parts.size() == 2);
}
