#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-partial: " name, "[client-partial]" VSSL)

TEST("response receive") {
    AsyncTest test(1000);
    ClientPair p(test.loop);

    ServerResponseSP sres;

    p.server->request_event.add([&](auto req) {
        sres = new ServerResponse(200, Header(), Body(), true);
        req->respond(sres);
    });

    auto req = Request::Builder().uri("/").build();

    size_t count = 10;
    req->partial_event.add([&](auto, auto res, auto err) {
        CHECK_FALSE(err);
        CHECK(res->state() >= State::got_header);
        if (count--) {
            CHECK(res->body.length() == 9 - count);
            sres->send_chunk("a");
            return;
        }

        sres->send_final_chunk();
        req->partial_event.remove_all();
        req->partial_event.add([&](auto, auto res, auto err) {
            CHECK_FALSE(err);
            CHECK(res->state() == State::done);
            CHECK(res->body.to_string() == "aaaaaaaaaa");
            //test.loop->stop();
        });
    });

    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
    CHECK(res->chunked);
    CHECK(res->state() == State::done);
    CHECK(res->body.to_string() == "aaaaaaaaaa");
}
