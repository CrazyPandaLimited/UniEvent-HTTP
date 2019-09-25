#include "../lib/test.h"

//TEST_CASE("pipeline", "[http-client][pipeline]") {
    //server::ServerSP echo;
    //uint16_t echo_port;
    //std::tie(echo, echo_port) = echo_server("pipeline");
    //echo->run();

    //string uri_str = "http://127.0.0.1:" + to_string(echo_port);
    //iptr<uri::URI> uri = make_iptr<uri::URI>(uri_str);

    //iptr<client::Connection> connection;

    //std::map<string, ResponseSP> responses;
    //const int REQUEST_COUNT = 10;
    //for(int i = 0; i < REQUEST_COUNT; ++i) {
        //string host = "host" + to_string(i);
        //auto request = client::Request::Builder()
            //.method(parser::Request::Method::GET)
            //.uri(uri)
            //.header(parser::Header::Builder()
                    //.host(host)
                    //.build())
            //.response_callback([host, &responses](client::RequestSP request, ResponseSP r) {
                    //responses.insert(std::make_pair(host, r));
                    //})
            //.build();

        //if(!connection) {
            //connection = http_request(request);
        //} else {
            //http_request(request, connection);
        //}
    //}

    //wait(500, Loop::default_loop());

    ////for(int i = 0; i < REQUEST_COUNT; ++i) {
        ////string host = "host" + to_string(i);
        ////auto response_pos = responses.find(host);
        ////REQUIRE(response_pos != std::end(responses));
        ////auto response = response_pos->second;
        ////auto echo_request = parse_request(response->body->as_buffer());

        ////REQUIRE(response);
        ////REQUIRE(response->is_valid());
        ////REQUIRE(response->http_version() == "1.1");

        ////responses.erase(response_pos);
    ////}
//}
