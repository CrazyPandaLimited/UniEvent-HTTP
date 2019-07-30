#include "../lib/test.h"

TEST_CASE("trivial pool", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_pool");

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

TEST_CASE("timeouted connection from pool", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_pool");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    client::ClientConnectionPool pool(Loop::default_loop(), 50);

    ResponseSP response1;
    client::RequestSP request1 = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response1 = r;
        })
        .timeout(100)
        .build();

    ResponseSP response2;
    client::RequestSP request2 = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response2 = r;
        })
        .timeout(100)
        .build();

    http_request(request1, &pool);
    http_request(request2, &pool);

    wait(100, Loop::default_loop());

    // all connections are expired
    REQUIRE(pool.empty());

    REQUIRE(response1);
    REQUIRE(response2);
    REQUIRE(response1->is_valid());
    REQUIRE(response2->is_valid());
}

TEST_CASE("sequential requests", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_pool");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    client::ClientConnectionPool pool(Loop::default_loop(), 50);

    ResponseSP response1;
    client::RequestSP request1 = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response1 = r;
        })
        .timeout(100)
        .build();

    ResponseSP response2;
    client::RequestSP request2 = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response2 = r;
        })
        .timeout(100)
        .build();

    http_request(request1, &pool);

    wait(100, Loop::default_loop());

    http_request(request2, &pool);

    wait(100, Loop::default_loop());

    // all connections are expired
    REQUIRE(pool.empty());

    REQUIRE(response1);
    REQUIRE(response2);
    REQUIRE(response1->is_valid());
    REQUIRE(response2->is_valid());
}

TEST_CASE("same request using pool", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("trivial_pool");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    client::ClientConnectionPool pool(Loop::default_loop(), 50);

    ResponseSP response;
    client::RequestSP request = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response = r;
        })
        .timeout(100)
        .build();

    // cant reuse not completed request
    REQUIRE_THROWS_AS([&]{
        http_request(request, &pool);
        http_request(request, &pool);
    }(), ProgrammingError);
}

TEST_CASE("timeouted pool request", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("timeouted_pool");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    string err;
    ResponseSP response;
    auto request = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .timeout(1)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response = r;
        })
        .error_callback([&](client::RequestSP, const string& details) {
            err = details;
        })
        .build();

    http_request(request);

    wait(200, Loop::default_loop());

    //TODO from time to time it manages to connect before the timeout
    //REQUIRE(!err.empty());

    err.clear();
    request = client::Request::Builder()
        .method(protocol::http::Request::Method::GET)
        .uri(uri)
        .timeout(100)
        .response_callback([&](client::RequestSP, ResponseSP r) {
            response = r;
        })
        .error_callback([&](client::RequestSP, const string& details) {
            err = details;
        })
        .build();

    http_request(request);

    wait(100, Loop::default_loop());

    REQUIRE(err.empty());
}

TEST_CASE("multiple pool requests", "[pool]") {
    server::ServerSP echo;
    uint16_t echo_port;
    std::tie(echo, echo_port) = echo_server("timeouted_pool");

    string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    // there is no guarantee that requests will be processed in order
    // because they will produce multiple different connections
    std::map<string, ResponseSP> responses;
    const int REQUEST_COUNT = 10;
    for(int i = 0; i < REQUEST_COUNT; ++i) {
        string host = "host" + to_string(i);
        auto request = client::Request::Builder()
            .method(protocol::http::Request::Method::GET)
            .uri(uri)
            .header(protocol::http::Header::Builder()
                    .host(host)
                    .build())
            .response_callback([host, &responses](client::RequestSP, ResponseSP r) {
                    responses.insert(std::make_pair(host, r));
                    })
            .build();

        http_request(request);
    }

    wait(100, Loop::default_loop());

    for(int i = 0; i < REQUEST_COUNT; ++i) {
        string host = "host" + to_string(i);
        auto response_pos = responses.find(host);
        REQUIRE(response_pos != std::end(responses));
        auto response = response_pos->second;
        auto echo_request = parse_request(response->body->as_buffer());

        REQUIRE(response);
        REQUIRE(response->is_valid());
        REQUIRE(response->http_version() == "1.1");

        responses.erase(response_pos);
    }
}
