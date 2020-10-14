#include "../lib/test.h"
using Catch::Matchers::Contains;
#include <iostream>

#define TEST(name) TEST_CASE("form: " name, "[form]")

static bool init_ssl() { secure = true; return secure; }

static bool _init_ssl = init_ssl();


TEST("form field") {
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);

    p.server->request_event.add([&](auto& req){
        // TODO: parse form
        test.happens();
        auto body = req->body.to_string();
        REQUIRE_THAT(body, Contains("secret"));
        REQUIRE_THAT(body, Contains("password"));
        ServerResponseSP res = new ServerResponse(200, Headers(), Body());
        req->respond(res);
    });


    auto req = Request::Builder().uri("/").form_field("password", "secret").build();
    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
}

TEST("embedded form file + field") {
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);

    p.server->request_event.add([&](auto& req){
        // TODO: parse form
        test.happens();
        auto body = req->body.to_string();
        REQUIRE_THAT(body, Contains("secret"));
        REQUIRE_THAT(body, Contains("password"));
        REQUIRE_THAT(body, Contains("resume"));
        REQUIRE_THAT(body, Contains("[pdf]"));
        REQUIRE_THAT(body, Contains("application/pdf"));
        REQUIRE_THAT(body, Contains("cv.pdf"));
        ServerResponseSP res = new ServerResponse(200, Headers(), Body());
        req->respond(res);
    });


    auto req = Request::Builder().uri("/")
            .form_field("password", "secret")
            .form_file("resume", "[pdf]", "application/pdf", "cv.pdf")
            .build();
    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
}

