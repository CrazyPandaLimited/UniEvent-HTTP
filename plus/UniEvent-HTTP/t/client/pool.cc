#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-pool: " name, "[client-pool]" VSSL)

TEST("reusing connection") {
    AsyncTest test(1000, 1);
    TPool p(test.loop);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200));
    srv->autorespond(new ServerResponse(200));

    CHECK_FALSE(p.size());
    CHECK_FALSE(p.nbusy());

    auto uri = active_scheme() +  "://" + srv->location() + "/";
    auto req = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c = p.request(req);
    REQUIRE(c);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    c->connect_event.add([&](auto...){ test.happens(); }); // should connect only once

    auto req1 =std::vector<RequestSP>{req};
    auto res = p.await_responses(req1);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    auto c2 = p.request(req); // should be the same client
    CHECK(c == c2);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    res = p.await_responses(req1);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);
}

TEST("reusing connection after c=close") {
    AsyncTest test(1000, 2);
    TPool p(test.loop);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200, Headers().connection("close")));
    srv->autorespond(new ServerResponse(200));

    auto uri = active_scheme() +  "://" + srv->location() + "/";
    auto req = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c = p.request(req);
    REQUIRE(c);

    c->connect_event.add([&](auto...){ test.happens(); }); // should connect twice

    auto req1 =std::vector<RequestSP>{req};
    auto res = p.await_responses(req1);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);
    CHECK_FALSE(res[0]->keep_alive());

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    auto c2 = p.request(req);
    CHECK(c == c2);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    res = p.await_responses(req1);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);
}


TEST("different servers") {
    AsyncTest test(1000, 2);
    TPool p(test.loop);
    auto srv1 = make_server(test.loop);
    auto srv2 = make_server(test.loop);
    srv1->autorespond(new ServerResponse(200));
    srv2->autorespond(new ServerResponse(200));

    auto uri1 = active_scheme() +  "://" + srv1->location() + "/";
    auto req1 = Request::Builder().method(Request::Method::GET).uri(uri1).build();
    auto c = p.request(req1);
    REQUIRE(c);
    c->connect_event.add([&](auto...){ test.happens(); });

    auto reqs1 =std::vector<RequestSP>{req1};
    auto res = p.await_responses(reqs1);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    auto uri2 = active_scheme() +  "://" + srv2->location() + "/";
    auto req2 = Request::Builder().method(Request::Method::GET).uri(uri2).build();
    auto c2 = p.request(req2);
    REQUIRE(c2);
    c2->connect_event.add([&](auto...){ test.happens(); });

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    auto reqs2 =std::vector<RequestSP>{req2};
    res = p.await_responses(reqs2);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 0);
}

TEST("several requests to the same server at once") {
    AsyncTest test(1000, 2);
    TPool p(test.loop);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200, Headers(), Body("1")));
    srv->autorespond(new ServerResponse(200, Headers(), Body("1")));
    srv->autorespond(new ServerResponse(200, Headers(), Body("2")));
    srv->autorespond(new ServerResponse(200, Headers(), Body("2")));

    auto uri = active_scheme() +  "://" + srv->location() + "/";
    auto r1 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c1 = p.request(r1);
    REQUIRE(c1);
    c1->connect_event.add([&](auto...){ test.happens(); });

    auto r2 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c2 = p.request(r2);
    REQUIRE(c2);
    c2->connect_event.add([&](auto...){ test.happens(); });

    CHECK(c1 != c2);
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 2);

    auto req1 =std::vector<RequestSP>{r1, r2};
    auto res1 = p.await_responses(req1);
    REQUIRE(res1.size() == 2);
    CHECK(res1[0]->code == 200);
    CHECK(res1[0]->body.to_string() == "1");
    CHECK(res1[1]->code == 200);
    CHECK(res1[1]->body.to_string() == "1");
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 0);


    auto r3 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c3 = p.request(r3);
    CHECK((c3 == c1 || c3 == c2));
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    auto r4 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c4 = c2 = p.request(r4);
    CHECK((c4 == c1 || c4 == c2));
    CHECK(c4 != c3);
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 2);

    auto req2 =std::vector<RequestSP>{r3, r4};
    auto res2 = p.await_responses(req2);
    REQUIRE(res2.size() == 2);
    CHECK(res2[0]->code == 200);
    CHECK(res2[0]->body.to_string() == "2");
    CHECK(res2[1]->code == 200);
    CHECK(res2[1]->body.to_string() == "2");
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 0);
}

TEST("idle timeout") {
    AsyncTest test(1000, 2);
    TPoolSP p;

    SECTION("set at creation time") {
        Pool::Config cfg;
        cfg.idle_timeout = 5;
        p = new TPool(test.loop, cfg);
    }
    SECTION("set at runtime") {
        p = new TPool(test.loop);
        p->idle_timeout(5);
    }

    auto srv = make_server(test.loop);

    auto uri = active_scheme() +  "://" + srv->location() + "/";
    auto req1 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c = p->request(req1);
    REQUIRE(c);
    c->connect_event.add([&](auto...){ test.happens(); });

    test.wait(15); // more than idle_timeout
    // client is busy and not affected by idle timeout
    CHECK(p->size() == 1);
    CHECK(p->nbusy() == 1);

    req1->cancel();

    test.wait(15);
    CHECK(p->size() == 0);
    CHECK(p->nbusy() == 0);

    srv->autorespond(new ServerResponse(200));
    auto req2 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    c = p->request(req2);
    REQUIRE(c);
    c->connect_event.add([&](auto...){ test.happens(); });
    CHECK(p->size() == 1);
    CHECK(p->nbusy() == 1);

    auto reqs2 =std::vector<RequestSP>{req2};
    auto res = p->await_responses(reqs2);
    REQUIRE(res.size() == 1);
    CHECK(res[0]->code == 200);
}

TEST("instance") {
    auto dpool = Pool::instance(Loop::default_loop());
    CHECK(dpool == Pool::instance(Loop::default_loop()));
    CHECK(dpool == Pool::instance(Loop::default_loop()));

    LoopSP l2 = new Loop();
    auto p2 = Pool::instance(l2);
    CHECK(p2 != dpool);
    CHECK(p2 == Pool::instance(l2));
}

TEST("http_request") {
    AsyncTest test(1000, 1);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200, Headers(), Body("hi")));

    auto uristr = (secure ? string("https://") : string("http://")) + srv->location() + '/';
    auto req = Request::Builder().uri(uristr).build();
    req->response_event.add([&](auto, auto res, auto err) {
        CHECK_FALSE(err);
        CHECK(res->body.to_string() == "hi");
        test.happens();
        test.loop->stop();
    });

    http_request(req, test.loop);

    auto pool = Pool::instance(test.loop);
    CHECK(pool->size() == 1);
    CHECK(pool->nbusy() == 1);

    test.run();
}

TEST("request/client continue to work fine after pool is unreferenced") {
    AsyncTest test(1000, 1);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200));

    auto req = Request::Builder().uri("/").response_callback([&](auto, auto res, auto err) {
        CHECK_FALSE(err);
        CHECK(res->code == 200);
        test.happens();
        test.loop->stop();
    }).build();
    req->uri->host(srv->sockaddr().ip());
    req->uri->port(srv->sockaddr().port());
    if (secure) req->uri->scheme("https");

    {
        Pool p(test.loop);
        p.request(req);
    }

    test.run();
}

TEST("connection queuing") {
    AsyncTest test(1000, 2);
    Pool::Config cfg;
    cfg.max_connections = 2;
    TPool p(test.loop, cfg);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200));
    srv->autorespond(new ServerResponse(200));
    srv->autorespond(new ServerResponse(200));

    auto uri = active_scheme() +  "://" + srv->location() + "/";
    auto r1 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto r2 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto r3 = Request::Builder().method(Request::Method::GET).uri(uri).build();
    auto c1 = p.request(r1);
    auto c2 = p.request(r2);
    auto c3 = p.request(r3);

    c1->connect_event.add([&](auto...){ test.happens(); });
    c2->connect_event.add([&](auto...){ test.happens(); });

    REQUIRE(c1);
    REQUIRE(c2);
    REQUIRE(!c3);

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 2);

    auto req1 =std::vector<RequestSP>{r1, r2};
    auto res1 = p.await_responses(req1);
    REQUIRE(res1.size() == 2);
    CHECK(res1[0]->code == 200);
    CHECK(res1[1]->code == 200);

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    auto req2 =std::vector<RequestSP>{r3};
    auto res2 = p.await_responses(req2);
    REQUIRE(res2.size() == 1);
    CHECK(res2[0]->code == 200);

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 0);
}
