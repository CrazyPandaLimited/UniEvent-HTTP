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

    auto c = p.get(srv->netloc());
    c->sa = srv->sockaddr();

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    c->connect_event.add([&](auto...){ test.happens(); }); // should connect only once

    auto res = c->get_response("/");
    CHECK(res->code == 200);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    auto c2 = p.get(srv->netloc()); // should be the same client
    CHECK(c == c2);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    res = c->get_response("/");
    CHECK(res->code == 200);
}

TEST("reusing connection after c=close") {
    AsyncTest test(1000, 2);
    TPool p(test.loop);
    auto srv = make_server(test.loop);
    srv->autorespond(new ServerResponse(200, Headers().connection("close")));
    srv->autorespond(new ServerResponse(200));

    auto c = p.get(srv->netloc());
    c->sa = srv->sockaddr();

    c->connect_event.add([&](auto...){ test.happens(); }); // should connect twice

    auto res = c->get_response("/");
    CHECK(res->code == 200);
    CHECK_FALSE(res->keep_alive());

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    auto c2 = p.get(srv->netloc());
    CHECK(c == c2);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 1);

    res = c->get_response("/");
    CHECK(res->code == 200);
}

TEST("different servers") {
    AsyncTest test(1000, 2);
    TPool p(test.loop);
    auto srv1 = make_server(test.loop);
    auto srv2 = make_server(test.loop);
    srv1->autorespond(new ServerResponse(200));
    srv2->autorespond(new ServerResponse(200));

    auto c = p.get(srv1->netloc());
    c->sa = srv1->sockaddr();
    c->connect_event.add([&](auto...){ test.happens(); });

    auto res = c->get_response("/");
    CHECK(res->code == 200);

    CHECK(p.size() == 1);
    CHECK(p.nbusy() == 0);

    c = p.get(srv2->netloc());
    c->sa = srv2->sockaddr();
    c->connect_event.add([&](auto...){ test.happens(); });

    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    res = c->get_response("/");
    CHECK(res->code == 200);

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

    auto c1 = p.get(srv->netloc());
    c1->sa = srv->sockaddr();
    c1->connect_event.add([&](auto...){ test.happens(); });

    auto c2 = p.get(srv->netloc());
    c2->sa = srv->sockaddr();
    c2->connect_event.add([&](auto...){ test.happens(); });

    CHECK(c1 != c2);
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 2);

    auto res = c2->get_response("/");
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "1");
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    res = c1->get_response("/");
    CHECK(res->code == 200);
    CHECK(res->body.to_string() == "1");
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 0);

    auto c3 = p.get(srv->netloc());
    CHECK((c3 == c1 || c3 == c2));
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 1);

    auto c4 = p.get(srv->netloc());
    CHECK((c4 == c1 || c4 == c2));
    CHECK(c4 != c3);
    CHECK(p.size() == 2);
    CHECK(p.nbusy() == 2);

    res = c1->get_response("/");
    CHECK(res->body.to_string() == "2");
    res = c2->get_response("/");
    CHECK(res->body.to_string() == "2");
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
    srv->autorespond(new ServerResponse(200));
    srv->autorespond(new ServerResponse(200));

    auto c = p->get(srv->netloc());
    c->sa = srv->sockaddr();
    c->connect_event.add([&](auto...){ test.happens(); });
    test.wait(15); // more than idle_timeout
    // client is busy and not affected by idle timeout
    CHECK(p->size() == 1);
    CHECK(p->nbusy() == 1);

    auto res = c->get_response("/");
    CHECK(res->code == 200);

    test.wait(15);
    CHECK(p->size() == 0);
    CHECK(p->nbusy() == 0);

    c = p->get(srv->netloc());
    c->sa = srv->sockaddr();
    c->connect_event.add([&](auto...){ test.happens(); });
    CHECK(p->size() == 1);
    CHECK(p->nbusy() == 1);
    res = c->get_response("/");
    CHECK(res->code == 200);
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
