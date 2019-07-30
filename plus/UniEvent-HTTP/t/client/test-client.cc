#include "../lib/test.h"

TEST_CASE("trivial get", "[http-client]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_get");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    protocol::http::ResponseSP response;
    protocol::http::RequestSP request;
    std::tie(response, request) = echo_request(uri, protocol::http::Request::Method::GET);

    REQUIRE(response);
    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    REQUIRE(request->headers.get_field("Host") == protocol::http::to_host(uri));
}

TEST_CASE("trivial post", "[http-client]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_post");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    string TEST_BODY = "Body";

    protocol::http::ResponseSP response;
    protocol::http::RequestSP request;
    std::tie(response, request) = echo_request(uri, protocol::http::Request::Method::POST, TEST_BODY);

    REQUIRE(response);
    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    REQUIRE(request->headers.get_field("Host") == protocol::http::to_host(uri));
    REQUIRE(request->body->as_buffer() == TEST_BODY);
}

TEST_CASE("request larger than mtu", "[http-client]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("large_request");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    // default loopback MTU is about 65K, so we need some more to have multiple reads
    string test_body = random_string_generator<string>(1024*1024);

    protocol::http::ResponseSP response;
    protocol::http::RequestSP request;
    std::tie(response, request) = echo_request(uri, protocol::http::Request::Method::POST, test_body);

    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    REQUIRE(request->headers.get_field("Host") == protocol::http::to_host(uri));
    REQUIRE(request->body->as_buffer() == test_body);
}

TEST_CASE("response larger than mtu", "[http-client]") {
    // assume that mtu is lower than our test body
    string test_body = random_string_generator<string>(1024*1024);
    server::ServerSP server = make_iptr<server::Server>();
    server->request_callback.add([&](server::ConnectionSP connection, protocol::http::RequestSP, ResponseSP& response) {
        response.reset(Response::Builder()
            .header(protocol::http::Header::Builder()
                .date(connection->server()->http_date_now())
                .build())
            .code(200)
            .reason("OK")
            .body(test_body)
            .build());
    });

    server::Server::Config config;
    config.locations.emplace_back(server::Location{"127.0.0.1"});
    config.dump_file_prefix = "dump.large_response";

    server->configure(config);
    server->run();

    string uri_str = "http://127.0.0.1:" + to_string(server->listeners[0]->sockaddr().port());
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    ResponseSP response;
    client::RequestSP request = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response = r;
        })
        .timeout(100)
        .build();

    http_request(request);

    wait(100, Loop::default_loop());

    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    REQUIRE(response->body->as_buffer() == test_body);
}

TEST_CASE("chunked response", "[http-client]") {
    auto chunk_timer = make_iptr<Timer>(Loop::default_loop());

    size_t pos = 0;
    size_t block_size = 4096;
    size_t body_size = 1024*1024;

    string test_body = random_string_generator<string>(body_size);

    server::ServerSP server = make_iptr<server::Server>();
    server->request_callback.add([&](server::ConnectionSP connection,
                protocol::http::RequestSP, ResponseSP& r) {
        r.reset(Response::Builder()
            .header(protocol::http::Header::Builder()
                .date(connection->server()->http_date_now())
                .chunked()
                .build())
            .code(200)
            .reason("OK")
            .build());

        chunk_timer->event.add([&pos, &test_body, &block_size, chunk_timer, r](auto&) {
            //panda_log_debug("ticking chunk timer: " << pos);
            if(pos >= test_body.length()) {
                //panda_log_debug("ticking chunk timer, stop");
                chunk_timer->stop();
            }

            if(test_body.length() - pos <= block_size) {
                r->write_chunk(test_body.substr(pos, test_body.length() - pos), true);
                chunk_timer->stop();
            } else {
                r->write_chunk(test_body.substr(pos, block_size));
            }

            pos += block_size;
        });

        chunk_timer->start(1);
    });

    server::Server::Config config;
    config.locations.emplace_back(server::Location{"127.0.0.1"});
    config.dump_file_prefix = "dump.chunked_response";
    server->configure(config);
    server->run();

    string uri_str = "http://127.0.0.1:" + to_string(server->listeners[0]->sockaddr().port());
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    client::Request::Builder builder = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .timeout(500);

    ResponseSP response;
    builder.response_callback([&](client::RequestSP, ResponseSP r) {
        response = r;
    });

    auto request = builder.build();

    http_request(request);

    wait(500, Loop::default_loop());

    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    REQUIRE(response->body->as_buffer() == test_body);
}

TEST_CASE("simple redirect", "[http-client]") {
    server::ServerSP to;
    uint16_t to_port;
    std::tie(to, to_port) = echo_server("redirect_to");

    string to_uri_str = "http://127.0.0.1:" + to_string(to_port);
    iptr<uri::URI> to_uri = make_iptr<uri::URI>(to_uri_str);

    uint16_t from_port;
    server::ServerSP from;
    std::tie(from, from_port) = redirect_server("from", to_port);

    string from_uri_str = "http://127.0.0.1:" + to_string(from_port);
    iptr<uri::URI> from_uri = make_iptr<uri::URI>(from_uri_str);

    protocol::http::ResponseSP response;
    protocol::http::RequestSP to_request;
    std::tie(response, to_request) = echo_request(from_uri, protocol::http::Request::Method::GET);

    REQUIRE(response);
    REQUIRE(response->is_valid());
    REQUIRE(response->http_version() == "1.1");
    // Host will be changed in the process of redirection
    REQUIRE(to_request->headers.get_field("Host") == protocol::http::to_host(to_uri));
}

//TEST_CASE("redirect chain", "[http-client]") {
    //server::ServerSP echo;
    //uint16_t echo_port;
    //std::tie(echo, echo_port) = echo_server("echo");

    //string echo_uri_str = "http://127.0.0.1:" + to_string(echo_port);
    //iptr<uri::URI> echo_uri = make_iptr<uri::URI>(echo_uri_str);

    //server::ServerSP redirect3;
    //uint16_t redirect3_port;
    //std::tie(redirect3, redirect3_port) = redirect_server("redirect3", echo_port);

    //string redirect3_uri_str = "http://127.0.0.1:" + to_string(redirect3_port);
    //iptr<uri::URI> redirect3_uri = make_iptr<uri::URI>(redirect3_uri_str);

    //server::ServerSP redirect2;
    //uint16_t redirect2_port;
    //std::tie(redirect2, redirect2_port) = redirect_server("redirect2", redirect3_port);

    //string redirect2_uri_str = "http://127.0.0.1:" + to_string(redirect2_port);
    //iptr<uri::URI> redirect2_uri = make_iptr<uri::URI>(redirect2_uri_str);

    //server::ServerSP redirect1;
    //uint16_t redirect1_port;
    //std::tie(redirect1, redirect1_port) = redirect_server("redirect1", redirect2_port);

    //string redirect1_uri_str = "http://127.0.0.1:" + to_string(redirect1_port);
    //iptr<uri::URI> redirect1_uri = make_iptr<uri::URI>(redirect1_uri_str);

    //protocol::http::ResponseSP response;
    //protocol::http::RequestSP request;
    //std::tie(response, request) = echo_request(redirect1_uri, protocol::http::Request::Method::GET);

    //REQUIRE(response);
    //REQUIRE(response->is_valid());
    //REQUIRE(response->http_version() == "1.1");
    //REQUIRE(request->headers.get_field("Host") == protocol::http::to_host(echo_uri));
//}

//TEST_CASE("redirection-loop", "[http-client]") {
    //uint16_t from_port = find_free_port();
    //uint16_t to_port = find_free_port();

    //server::ServerSP from_redirect;
    //uint16_t from_port;
    //std::tie(from_redirect, from_port) = redirect_server("from_redirect", to_port);

    //server::ServerSP from_redirect;
    //uint16_t from_port;
    //std::tie(to_redirect, to_port) = redirect_server("to_redirect", from_port);

    //iptr<uri::URI> from_uri = make_iptr<uri::URI>("http://127.0.0.1:" + to_string(from_port));

    //string uri_str = "http://127.0.0.1:" + to_string(from_port);
    //iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    //string err;
    //ResponseSP response;
    //auto request = client::Request::Builder()
        //.method(protocol::http::Request::Method::GET)
        //.uri(uri)
        //.timeout(500)
        //.response_callback([&](client::RequestSP, ResponseSP r) {
            //response = r;
        //})
        //.error_callback([&](client::RequestSP, const string& details) {
            //err = details;
        //})
        //.build();

    //http_request(request);

    //wait(500, Loop::default_loop());

    //REQUIRE(!err.empty());
//}
