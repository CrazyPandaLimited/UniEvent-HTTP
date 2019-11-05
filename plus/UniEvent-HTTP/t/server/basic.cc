#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-basic: " name, "[server-basic]" VSSL)

TEST("request without body") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        CHECK(req->method == Request::Method::GET);
        CHECK(req->state() == State::done);
        CHECK(req->headers.host() == "epta.ru");
        CHECK(!req->body.length());
        test.loop->stop();
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    test.run();
}

TEST("request with body") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        CHECK(req->method == Request::Method::POST);
        CHECK(req->state() == State::done);
        CHECK(req->body.to_string() == "epta nah");
        test.loop->stop();
    });

    p.conn->write(
        "POST / HTTP/1.1\r\n"
        "Content-length: 8\r\n"
        "\r\n"
        "epta nah"
    );

    test.run();
}

TEST("request with chunks") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        CHECK(req->method == Request::Method::POST);
        CHECK(req->state() == State::done);
        CHECK(req->body.to_string() == "1234567891234567");
        test.loop->stop();
    });

    p.conn->write(
        "POST / HTTP/1.1\r\n"
        "TrAnsfeR-EncoDing: ChUnKeD\r\n"
        "\r\n"
        "9\r\n"
        "123456789\r\n"
        "7\r\n"
        "1234567\r\n"
        "0\r\n\r\n"
    );

    test.run();
}

TEST("response without body") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        req->respond(new ServerResponse(200));
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(!res->body.length());
}

TEST("response with body") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        req->respond(new ServerResponse(200, Header(), Body("epta-epta")));
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "epta-epta");
}

TEST("response with chunks") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        ServerResponseSP res = new ServerResponse(200);
        res->chunked = true;
        res->body.parts.push_back("epta raz");
        res->body.parts.push_back("epta dva");
        req->respond(res);
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "epta razepta dva");
    CHECK(res->body.parts.size() == 2);
    CHECK(res->chunked);
}

TEST("delayed response") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        test.loop->delay([req]{
            req->respond(new ServerResponse(200));
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(!res->body.length());
}

TEST("response with delayed chunks") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    p.server->request_event.add([&](auto req) {
        test.happens();
        req->respond(new ServerResponse(200, Header(), Body(), true));
        test.loop->delay([&, req]{
            req->response()->send_chunk("1 ");
            test.loop->delay([&, req]{
                req->response()->send_chunk("2");
                test.loop->delay([&, req]{
                    req->response()->send_final_chunk();
                });
            });
        });
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.length() == 3);
    CHECK(res->body.parts.size() == 2);
    CHECK(res->body.to_string() == "1 2");
    CHECK(res->chunked);
}

TEST("request parsing error") {
    AsyncTest test(1000, 1);
    auto p = make_server_pair(test.loop);

    int check_code;
    SECTION("automatic error response")     { check_code = 400; }
    SECTION("user's custom error response") { check_code = 404; }

    p.server->error_event.add([&](auto& req, auto& err){
        test.happens();
        CHECK(req->headers.host() == "epta.ru");
        CHECK(err == errc::parse_error);
        if (check_code != 400) req->respond(new ServerResponse(check_code));
    });

    p.server->request_event.add(fail_cb);

    auto res = p.get_response(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "something not looking like chunk"
    );
    CHECK(res->code == check_code);
}

TEST("request drop event when client disconnects and response not yet completed") {
    AsyncTest test(1000, 2);
    auto p = make_server_pair(test.loop);

    SECTION("no response at all") {}
    SECTION("partial response") {
        p.autorespond(new ServerResponse(200, Header(), Body(), true));
    }

    p.server->request_event.add([&](auto& req){
        test.happens();
        req->drop_event.add([&](auto& req, auto& err){
            test.happens();
            CHECK(err == std::errc::connection_reset);
            CHECK(req->headers.host() == "epta.ru");
            test.loop->stop();
        });
        p.conn->disconnect();
    });

    p.conn->write(
        "GET / HTTP/1.1\r\n"
        "Host: epta.ru\r\n"
        "\r\n"
    );

    test.run();
}

TEST("date header") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);

    for (int i = 0; i < 2; ++i) {
        p.autorespond(new ServerResponse(200));
        auto res = p.get_response("GET / HTTP/1.1\r\n\r\n");
        auto s = res->headers.date();
        CHECK(s);
    }
}

TEST("max headers size") {
    AsyncTest test(1000);
    Server::Config cfg;
    SECTION("allowed") { cfg.max_headers_size = 15; }
    SECTION("denied")  { cfg.max_headers_size = 14; }
    auto p = make_server_pair(test.loop, cfg);
    p.autorespond(new ServerResponse(200));

    auto res = p.get_response(
        "GET / HTTP/1.1\r\n"
        "Header: value\r\n" // len=15
        "\r\n"
    );

    if (cfg.max_headers_size == 15) {
        CHECK(res->code == 200);
    } else {
        CHECK(res->code == 400);
    }
}

TEST("max body size") {
    AsyncTest test(1000);
    Server::Config cfg;
    SECTION("allowed") { cfg.max_body_size = 10; }
    SECTION("denied")  { cfg.max_body_size = 9; }
    auto p = make_server_pair(test.loop, cfg);
    p.autorespond(new ServerResponse(200));

    auto res = p.get_response(
        "GET / HTTP/1.1\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "0123456789"
    );

    if (cfg.max_body_size == 10) {
        CHECK(res->code == 200);
    } else {
        CHECK(res->code == 400);
    }
}
