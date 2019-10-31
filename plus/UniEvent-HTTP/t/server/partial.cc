#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-partial: " name, "[server-partial]" VSSL)

TEST("request receive") {
    AsyncTest test(1000, 5);
    auto p = make_server_pair(test.loop);

    p.server->route_event.add([&](auto& req){
        test.happens();
        req->enable_partial();
        req->receive_event.add(fail_cb);
        req->partial_event.add([&](auto& req, auto& err) {
            test.happens();
            CHECK(!err);
            auto body = req->body.to_string();
            if (!body) {
                CHECK(req->state() == State::in_body);
                p.conn->write("1");
            }
            else if (body == "1") {
                CHECK(req->state() == State::in_body);
                p.conn->write("2");
            } else if (body == "12") {
                CHECK(req->state() == State::in_body);
                p.conn->write("3");
            } else if (body == "123") {
                CHECK(req->state() == State::done);
                req->respond(new ServerResponse(200, Header(), Body("epta")));
            }
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "epta");
}

TEST("full response on route event") {
    AsyncTest test(1000, 3);
    auto p = make_server_pair(test.loop);

    p.server->route_event.add([&](auto& req) {
        test.happens();
        req->enable_partial();
        req->partial_event.add([&](auto& req, auto& err) {
            test.happens();
            CHECK(!err);
            if (req->state() == State::done) test.loop->stop();
        });
        req->respond(new ServerResponse(302, Header(), Body("route-res")));
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 302);
    CHECK(res->body.to_string() == "route-res");

    p.conn->write("epta");
    test.run();
}

TEST("full response on partial event when request is not yet fully parsed") {
    AsyncTest test(1000, 3);
    auto p = make_server_pair(test.loop);

    p.server->route_event.add([&](auto& req) {
        test.happens();
        req->enable_partial();
        req->partial_event.add([&](auto& req, auto& err) {
            test.happens();
            CHECK(!err);
            if (req->state() == State::done) test.loop->stop();
            else req->respond(new ServerResponse(200, Header(), Body("partial-res")));
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "partial-res");

    p.conn->write("epta");
    test.run();
}

TEST("client disconnects or request error while in partial mode") {
    AsyncTest test(1000, 2);
    auto p = make_server_pair(test.loop);

    bool send_junk = false;
    bool partial_response = false;
    SECTION("client disconnects") { }
    SECTION("parsing error") { send_junk = true; }
    SECTION("partial response") { partial_response = true; }

    p.server->error_event.add(fail_cb);

    p.server->route_event.add([&](auto& req) {
        req->enable_partial();
        req->drop_event.add(fail_cb);
        req->partial_event.add([&](auto& req, auto& err) {
            test.happens();
            CHECK(!err);
            CHECK(req->state() != State::done);
            req->partial_event.remove_all();
            req->partial_event.add([&](auto&, auto& err) {
                test.happens();
                if (send_junk) CHECK(err == errc::parse_error);
                else           CHECK(err == std::errc::connection_reset);
                test.loop->stop();
            });
            if (partial_response) req->respond(new ServerResponse(200, Header(), Body(), true));
            if (send_junk) p.conn->write("something not looking like chunk");
            else           p.conn->disconnect();
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
    );

    test.run();
}

TEST("client disconnects when partial mode is finished") {
    AsyncTest test(1000, 3);
    auto p = make_server_pair(test.loop);

    p.server->error_event.add(fail_cb);

    p.server->route_event.add([&](auto& req) {
        req->enable_partial();
        req->drop_event.add([&](auto...){
            test.happens();
            test.loop->stop();
        });
        req->partial_event.add([&](auto& req, auto& err) {
            test.happens();
            CHECK(!err);
            CHECK(req->state() != State::done);
            p.conn->write("1234");
            req->partial_event.remove_all();
            req->partial_event.add([&](auto& req, auto& err) {
                test.happens();
                CHECK(!err);
                CHECK(req->state() == State::done);
                req->partial_event.remove_all();
                req->partial_event.add(fail_cb);
                p.conn->disconnect();
            });
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
    );

    test.run();
}
