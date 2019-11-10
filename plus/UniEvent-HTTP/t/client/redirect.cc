#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-redirect: " name, "[client-redirect]" VSSL)

TEST("same server") {
    AsyncTest test(1000, {"connect", "redirect"});
    ClientPair p(test.loop);

    p.server->request_event.add([&](auto req){
        if (req->uri->to_string() == "/") {
            req->redirect("/index");
        } else if (req->uri->to_string() == "/index") {
            req->respond(new ServerResponse(200, Header().add("h", req->headers.get("h")), Body(req->body)));
        }
    });

    p.client->connect_event.add([&](auto...){ test.happens("connect"); });

    auto req = Request::Builder().uri("/").header("h", "v").body("b").build();
    req->redirect_event.add([&](auto req, auto res, auto uri){
        test.happens("redirect");
        CHECK(res->code == 302);
        CHECK(res->headers.location() == "/index");
        CHECK(uri->path() == "/index");
        CHECK(req->uri->path() == "/");
        CHECK(req->original_uri() == req->uri);
    });

    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
    CHECK(res->headers.get("h") == "v");
    CHECK(res->body.to_string() == "b");
    CHECK(req->uri->path() == "/index");
    CHECK(req->original_uri()->path() == "/");
}

TEST("different server") {
    AsyncTest test(1000, {"connect", "redirect", "connect"});
    ClientPair p(test.loop);
    auto backend = make_server(test.loop);
    backend->enable_echo();

    p.server->request_event.add([&](auto req){
        auto uri = req->uri;
        uri->host(backend->sockaddr().ip());
        uri->port(backend->sockaddr().port());
        req->redirect(uri);
    });

    p.client->connect_event.add([&](auto...){ test.happens("connect"); });

    auto req = Request::Builder().uri("/").header("h", "v").body("b").build();
    req->redirect_event.add([&](auto, auto res, auto uri){
        test.happens("redirect");
        CHECK(res->code == 302);
        auto check_uri = string("//") + backend->location() + '/';
        CHECK(res->headers.location() == check_uri);
        CHECK(uri->to_string() == (secure ? string("https:") : string("http:")) + check_uri);
    });

    auto res = p.client->get_response(req);
    CHECK(res->code == 200);
    CHECK(res->headers.get("h") == "v");
    CHECK(res->body.to_string() == "b");
}

TEST("redirection limit") {
    AsyncTest test(1000);

    auto req = Request::Builder().uri("/").build();

    int count = 10;
    SECTION("beyond limit") { req->redirection_limit = count; }
    SECTION("above limit")  { req->redirection_limit = count - 1; }
    SECTION("disallowed")   { req->redirection_limit = 0; }

    std::vector<TServerSP> backends = { make_server(test.loop) };
    backends[0]->autorespond(new ServerResponse(404));

    for (int i = 1; i <= count; ++i) {
        auto srv = make_server(test.loop);
        srv->request_event.add([&, i](auto req){
            auto uri = req->uri;
            uri->host(backends[i-1]->sockaddr().ip());
            uri->port(backends[i-1]->sockaddr().port());
            req->redirect(uri);
        });
        backends.push_back(srv);
    }

    TClientSP client = new TClient(test.loop);
    client->sa = backends.back()->sockaddr();

    int rcnt = 0;
    req->redirect_event.add([&](auto...){ rcnt++; });

    if (req->redirection_limit == count) {
        auto res = client->get_response(req);
        CHECK(res->code == 404);
        CHECK(rcnt == count);
    } else if (req->redirection_limit) {
        auto err = client->get_error(req);
        CHECK(err == errc::redirection_limit);
        CHECK(rcnt == count - 1);
    } else {
        auto err = client->get_error(req);
        CHECK(err == errc::unexpected_redirect);
        CHECK(rcnt == 0);
    }
}

TEST("do not follow redirections") {
    AsyncTest test(1000);
    ClientPair p(test.loop);

    p.server->autorespond(new ServerResponse(302, Header().location("http://ya.ru")));

    auto req = Request::Builder().uri("/").follow_redirect(false).build();
    auto res = p.client->get_response(req);
    CHECK(res->code == 302);
    CHECK(res->headers.get("Location") == "http://ya.ru");
}

TEST("timeout") { // timeout is for whole request including all redirections
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);

    p.server->request_event.add([&](auto req){
        if (req->uri->to_string() == "/") {
            req->redirect("/index");
        }
    });

    auto req = Request::Builder().uri("/").timeout(5).build();
    req->redirect_event.add([&](auto...){ test.happens(); });

    auto err = p.client->get_error(req);
    CHECK(err == std::errc::timed_out);
}