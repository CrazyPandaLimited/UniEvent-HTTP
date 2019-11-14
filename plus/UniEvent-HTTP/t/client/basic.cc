#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-basic: " name, "[client-basic]" VSSL)

TEST("trivial get") {
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);
    p.server->enable_echo();

    p.server->request_event.add([&](auto& req){
        test.happens();
        auto sa = p.server->listeners().front()->sockaddr();
        CHECK(req->headers.get("Host") == sa.ip() + ':' + panda::to_string(sa.port()));
    });

    auto res = p.client->get_response("/", Header().add("Hello", "world"));

    CHECK(res->code == 200);
    CHECK(res->http_version == 11);
    CHECK(res->headers.get("Hello") == "world");
}

TEST("trivial post") {
    AsyncTest test(1000);
    ClientPair p(test.loop);
    p.server->enable_echo();

    auto res = p.client->get_response(Request::Builder().method(Request::Method::POST).uri("/").body("hello").build());

    CHECK(res->code == 200);
    CHECK(res->http_version == 11);
    CHECK(res->body.to_string() == "hello");
}

TEST("request and response larger than mtu") {
    AsyncTest test(1000);
    ClientPair p(test.loop);
    p.server->enable_echo();

    size_t sz = 1024*1024;
    string body(sz);
    for (size_t i = 0; i < sz; ++i) body[i] = 'a';

    auto res = p.client->get_response(Request::Builder().method(Request::Method::POST).uri("/").body(body).build());

    CHECK(res->code == 200);
    CHECK(res->body.to_string() == body);
}

TEST("timeout") {
    AsyncTest test(1000);
    ClientPair p(test.loop);

    auto err = p.client->get_error(Request::Builder().uri("/").timeout(5).build());
    CHECK(err == std::errc::timed_out);
}

TEST("client retains until request is complete") {
    AsyncTest test(1000);
    auto srv = make_server(test.loop);
    srv->enable_echo();

    static int dcnt;
    dcnt = 0;

    struct MyClient : TClient {
        using TClient::TClient;

        ~MyClient () { dcnt++; }
    };

    TClientSP client = new MyClient(test.loop);
    client->sa = srv->sockaddr();

    auto req = Request::Builder().uri("/").body("hi").build();
    req->response_event.add([&](auto, auto res, auto err) {
        CHECK_FALSE(err);
        REQUIRE(res);
        CHECK(res->body.to_string() == "hi");
        test.loop->stop();
    });

    client->request(req);

    client.reset();
    CHECK(dcnt == 0);
    test.run();
    CHECK(dcnt == 1);
}
