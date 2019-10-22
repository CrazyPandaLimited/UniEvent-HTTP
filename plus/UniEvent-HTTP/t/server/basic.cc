#include "../lib/test.h"

#define TGROUP "[server-basic]"

TEST_CASE("server request without body", TGROUP VSSL) {
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

TEST_CASE("server request with body", TGROUP VSSL) {
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

TEST_CASE("server request with chunks", TGROUP VSSL) {
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

TEST_CASE("server response without body", TGROUP VSSL) {
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

TEST_CASE("server response with body", TGROUP VSSL) {
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

TEST_CASE("server response with chunks", TGROUP VSSL) {
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

TEST_CASE("server delayed response", TGROUP VSSL) {
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

TEST_CASE("server response with delayed chunks", TGROUP VSSL) {
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
