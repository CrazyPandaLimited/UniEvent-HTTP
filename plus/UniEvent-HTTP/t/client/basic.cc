#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-basic: " name, "[client-basic]" VSSL)

TEST("trivial get") {
    AsyncTest test(1000, 1);
    ClientPair p(test.loop);
    p.server->enable_echo();

    p.server->request_event.add([&](auto& req){
        test.happens();
        auto sa = p.server->listeners().front()->sockaddr();
        CHECK(req->headers.get("Host") == sa.ip() + ':' + panda::to_string(sa.port()));
    });

    auto res = p.client->get_response("/", Header().add("Hello", "world"));

    CHECK(res->code == 200);
    CHECK(res->http_version == 11);
    CHECK(res->headers.get("Hello") == "world");
}

TEST("trivial post") {
    AsyncTest test(1000);
    ClientPair p(test.loop);
    p.server->enable_echo();

    auto res = p.client->get_response(Request::Builder().method(Request::Method::POST).uri("/").body("hello").build());

    CHECK(res->code == 200);
    CHECK(res->http_version == 11);
    CHECK(res->body.to_string() == "hello");
}

TEST("request and response larger than mtu") {
    AsyncTest test(1000);
    ClientPair p(test.loop);
    p.server->enable_echo();

    size_t sz = 1024*1024;
    string body(sz);
    for (size_t i = 0; i < sz; ++i) body[i] = 'a';

    auto res = p.client->get_response(Request::Builder().method(Request::Method::POST).uri("/").body(body).build());

    CHECK(res->code == 200);
    CHECK(res->body.to_string() == body);
}

TEST("timeout") {
    AsyncTest test(1000);
    ClientPair p(test.loop);

    auto err = p.client->get_error(Request::Builder().uri("/").timeout(5).build());
    CHECK(err == std::errc::timed_out);
}

//TEST_CASE("chunked response", "[client-basic]") {
//    auto chunk_timer = make_iptr<Timer>(Loop::default_loop());
//
//    size_t pos = 0;
//    size_t block_size = 4096;
//    size_t body_size = 1024*1024;
//
//    string test_body = random_string_generator<string>(body_size);
//
//    server::ServerSP server = make_iptr<server::Server>();
//    server->request_callback.add([&](server::ConnectionSP connection,
//                protocol::http::RequestSP, ResponseSP& r) {
//        r.reset(Response::Builder()
//            .header(protocol::http::Header().date(connection->server()->http_date_now()).chunked())
//            .code(200)
//            .reason("OK")
//            .build());
//
//        chunk_timer->event.add([&pos, &test_body, &block_size, chunk_timer, r](auto&) {
//            //panda_log_debug("ticking chunk timer: " << pos);
//            if(pos >= test_body.length()) {
//                //panda_log_debug("ticking chunk timer, stop");
//                chunk_timer->stop();
//            }
//
//            if(test_body.length() - pos <= block_size) {
//                r->write_chunk(test_body.substr(pos, test_body.length() - pos), true);
//                chunk_timer->stop();
//            } else {
//                r->write_chunk(test_body.substr(pos, block_size));
//            }
//
//            pos += block_size;
//        });
//
//        chunk_timer->start(1);
//    });
//
//    server::Server::Config config;
//    config.locations.emplace_back(server::Location{"127.0.0.1"});
//    config.dump_file_prefix = "dump.chunked_response";
//    server->configure(config);
//    server->run();
//
//    string uri_str = "http://127.0.0.1:" + to_string(server->listeners[0]->sockaddr().port());
//    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);
//
//    client::Request::Builder builder = client::Request::Builder()
//        .method(protocol::http::Request::Method::GET)
//        .uri(uri)
//        .timeout(500);
//
//    ResponseSP response;
//    builder.response_callback([&](client::RequestSP, ResponseSP r) {
//        response = r;
//    });
//
//    auto request = builder.build();
//
//    http_request(request);
//
//    wait(500, Loop::default_loop());
//
//    REQUIRE(response->is_valid());
//    REQUIRE(response->http_version() == "1.1");
//    REQUIRE(response->body->as_buffer() == test_body);
//}
