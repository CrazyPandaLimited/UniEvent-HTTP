#include "../lib/test.h"

#define TEST(name) TEST_CASE("server-pipeline: " name, "[server-pipeline]" VSSL)

TEST("basic") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);

    bool async = false, reverse = false;
    SECTION("sync response") {}
    SECTION("async response, same order") { async = true; }
    SECTION("async response, reverse order") { async = true; reverse = true; }

    std::vector<ServerRequestSP> reqs;

    p.server->request_event.add([&](auto& req) {
        if (!async) {
            req->respond(new ServerResponse(200, Header(), Body(req->uri->path())));
            return;
        }

        reqs.push_back(req);
        if (reqs.size() < 3) return;

        test.loop->delay([&]{
            if (!reverse) {
                for (auto req : reqs) {
                    req->respond(new ServerResponse(200, Header(), Body(req->uri->path())));
                }
            } else {
                for (auto it = reqs.rbegin(); it != reqs.rend(); ++it) {
                    auto req = *it;
                    req->respond(new ServerResponse(200, Header(), Body(req->uri->path())));
                }
            }
        });
    });

    p.conn->write(
        "GET /a HTTP/1.1\r\n\r\n"
        "GET /b HTTP/1.1\r\n\r\n"
        "GET /c HTTP/1.1\r\n\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "/a");

    res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "/b");

    res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "/c");
}

TEST("chunked response captured and sent later") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);

    bool full;
    string check = "1234";
    SECTION("full chunked response") { full = true;}
    SECTION("partial chunked response") { full = false; check += "56"; }

    ServerRequestSP req1;
    p.server->request_event.add([&](auto& req) {
        if (!req1) {
            req1 = req;
            return;
        }

        auto res = new ServerResponse(200, Header(), Body(), true);
        req->respond(res);
        res->send_chunk("12");
        res->send_chunk("34");
        if (full) res->send_final_chunk();

        test.loop->delay([&, res]{
            req1->respond(new ServerResponse(302, Header(), Body("000")));
            if (!full) {
                res->send_chunk("56");
                res->send_final_chunk();
            }
        });
    });

    p.conn->write(
        "GET /a HTTP/1.1\r\n\r\n"
        "GET /b HTTP/1.1\r\n\r\n"
    );

    auto res = p.get_response();
    CHECK(res->code == 302);
    CHECK(res->body.to_string() == "000");

    res = p.get_response();
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == check);
}

TEST("request connection close") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);

    bool chunked = GENERATE(false, true);
    SECTION(chunked ? "response chunked" : "response full") {}

    bool second = false;
    p.server->request_event.add([&](auto& req) {
        if (!second) {
            req->respond(new ServerResponse(200, Header(), Body("ans1")));
            second = true;
            return;
        }

        if (!chunked) {
            req->respond(new ServerResponse(200, Header(), Body("ans2")));
            return;
        } else {
            req->respond(new ServerResponse(200, Header(), Body(), true));
            test.loop->delay([&, req]{
                req->response()->send_chunk("ans2");
                test.loop->delay([&, req]{
                    req->response()->send_final_chunk();
                });
            });
        }
    });

    p.conn->write(
        "GET /a HTTP/1.1\r\n\r\n"

        "GET /b HTTP/1.1\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->body.to_string() == "ans1");
    res = p.get_response();
    CHECK(res->body.to_string() == "ans2");

    CHECK(p.wait_eof(50));
}

TEST("request connection close waits until all previous requests are done") {
    AsyncTest test(1000);
    auto p = make_server_pair(test.loop);

    bool chunked1 = GENERATE(false, true);
    bool chunked2 = GENERATE(false, true);
    SECTION(string("chunked first-second response: ") + char(chunked1 + '0') + '-' + char(chunked2 + '0')){}

    ServerRequestSP req1;

    auto req1_ans = [&]{
        if (chunked1) {
            req1->respond(new ServerResponse(200, Header(), Body(), true));
            test.loop->delay([&]{
                req1->response()->send_chunk("ans1");
                test.loop->delay([&]{
                    req1->response()->send_final_chunk();
                });
            });
        }
        else req1->respond(new ServerResponse(200, Header(), Body("ans1")));
    };

    p.server->request_event.add([&](auto& req) {
        if (!req1) { req1 = req; return; }
        test.loop->delay([&, req]{
            if (chunked2) {
                req->respond(new ServerResponse(200, Header(), Body(), true));
                test.loop->delay([&, req]{
                    req->response()->send_chunk("ans2");
                    test.loop->delay([&, req]{
                        req->response()->send_final_chunk();
                        test.loop->delay(req1_ans);
                    });
                });
            } else {
                req->respond(new ServerResponse(200, Header(), Body("ans2")));
                test.loop->delay(req1_ans);
            }
        });
    });

    p.conn->write(
        "GET /a HTTP/1.1\r\n\r\n"

        "GET /b HTTP/1.1\r\n"
        "Connection: close\r\n"
        "\r\n"
    );

    auto res = p.get_response();
    CHECK(res->body.to_string() == "ans1");
    res = p.get_response();
    CHECK(res->body.to_string() == "ans2");

    CHECK(p.wait_eof(50));
}
