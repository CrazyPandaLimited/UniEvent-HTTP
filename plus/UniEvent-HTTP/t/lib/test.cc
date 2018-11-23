#include "test.h"

iptr<protocol::http::Request> parse_request(const string& buf) {
    auto parser = make_iptr<protocol::http::RequestParser>();
    protocol::http::RequestParser::Result result = parser->parse_first(buf);
    panda_log_debug("parse_request state [" << static_cast<int>(result.state) << "]"
<< " " << result.position);
    if(result.state == protocol::http::RequestParser::State::failed) {
        throw std::runtime_error("request parser failed");
    }

    if(!result.request->is_valid()) {
        throw std::runtime_error("request parser failed, invalid message");
    }

    return result.request;
}

// Send in request end get it back in reponse body
std::tuple<protocol::http::ResponseSP, protocol::http::RequestSP> echo_request(
        iptr<uri::URI> uri,
        protocol::http::Request::Method request_method,
        const string& body,
        protocol::http::HeaderSP header) {

    client::Request::Builder builder = client::Request::Builder()
        .method(request_method)
        .uri(uri)
        .timeout(100);
    
    if(!header->empty()) {
        builder.header(header);
    }
   
    if(!body.empty()) {
        builder.body(body);
    }
    
    client::ResponseSP response;
    builder.response_callback([&](client::RequestSP, client::ResponseSP r) {
        response = r;
    });

    auto request = builder.build();

    http_request(request);

    wait(200, Loop::default_loop());
   
    if(response && response->is_valid()) {
        auto echo_request = parse_request(response->body()->as_buffer());
        return std::make_tuple(response, echo_request);
    } else {
        return std::make_tuple(nullptr, nullptr);
    }
}

std::tuple<server::ServerSP, uint16_t> echo_server(const string& name) {

    auto server = make_iptr<server::Server>();
    server->request_callback.add([&](server::ConnectionSP connection, protocol::http::RequestSP request, server::ResponseSP& response) {
        std::stringstream ss;
        ss << request;
        string request_str(ss.str().c_str());

        panda_log_info("on_request, echoing back: [" << request_str << "]");

        response.reset(server::Response::Builder()
            .header(protocol::http::Header::Builder()
                .date(connection->server()->http_date_now())
                .build())
            .code(200)
            .reason("OK")
            .body(request_str)
            .build()); 
    });
    
    server::Server::Config config;
    config.locations.emplace_back(server::Location{"localhost"});
    config.dump_file_prefix = "echo." + name;
    server->configure(config);
    server->run();

    return std::make_tuple(server, server->listeners[0]->get_sockaddr().port());
}

std::tuple<server::ServerSP, uint16_t> redirect_server(const string& name, uint16_t to_port) {
    string location = "http://localhost:" + to_string(to_port); 
    auto server = make_iptr<server::Server>();
    server->request_callback.add([location, server](server::ConnectionSP /* connection */, 
                protocol::http::RequestSP /* request */, 
                server::ResponseSP& response) {
        //panda_log_debug("request_callback: " << request);
        response.reset(server::ResponseSP(server::Response::Builder()
            .header(protocol::http::Header::Builder()
                .date(server->http_date_now())
                .location(location)
                .build())
            .code(301)
            .reason("Moved Permanently")
            .body("<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\"" 
                + location + "\">" + location + "</a>.</p></body></html>",
                "text/html")
            .build())); 
    });

    server::Server::Config config;
    config.locations.emplace_back(server::Location{"localhost"});
    config.dump_file_prefix = "redirect." + name;
    server->configure(config);
    server->run();

    return std::make_tuple(server, server->listeners[0]->get_sockaddr().port());
}
