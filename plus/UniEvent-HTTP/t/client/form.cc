#include "../lib/test.h"
#include <iostream>

#define TEST(name) TEST_CASE("form: " name, "[form]")

static bool init_ssl() { secure = true; return secure; }

static bool _init_ssl = init_ssl();


TEST("simple form field") {
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);

    p.server->request_event.add([&](auto& req){
        test.happens();
        CHECK(req->body.to_string().find("\r\nsecret\r\n") != string::npos);
        ServerResponseSP res = new ServerResponse(200, Headers(), Body());
        req->respond(res);
    });


    auto req = Request::Builder().uri("/").form_field("password", "secret").build();
    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
}
