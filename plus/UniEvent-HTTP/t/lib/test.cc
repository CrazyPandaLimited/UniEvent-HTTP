#include "test.h"
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>

using panda::unievent::Timer;
using panda::unievent::TimerSP;

bool secure = false;

SSL_CTX* get_ssl_ctx () {
    static SSL_CTX* ctx = nullptr;
    if (ctx) return ctx;
    ctx = SSL_CTX_new(SSLv23_server_method());
    SSL_CTX_use_certificate_file(ctx, "t/cert/cert.pem", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "t/cert/key.pem", SSL_FILETYPE_PEM);
    SSL_CTX_check_private_key(ctx);
    return ctx;
}

TServerSP make_server (const LoopSP& loop, Server::Config cfg) {
    TServerSP server = new TServer(loop);

    Server::Location loc;
    loc.host = "127.0.0.1";
    if (secure) loc.ssl_ctx = get_ssl_ctx();
    cfg.locations.push_back(loc);
    server->configure(cfg);

    server->run();
    return server;
}

void TServer::enable_echo () {
    request_event.remove_all();
    request_event.add([](auto& req){
        auto h = std::move(req->headers);
        h.remove("Host");
        req->respond(new ServerResponse(200, std::move(h), Body(req->body.to_string())));
    });
}

void TServer::autorespond (const ServerResponseSP& res) {
    if (!autores) {
        autores = true;
        request_event.add([this](auto& req){
            if (!autoresponse_queue.size()) return;
            auto res = autoresponse_queue.front();
            autoresponse_queue.pop_front();
            req->respond(res);
        });
    }
    autoresponse_queue.push_back(res);
}

string TServer::location () const {
    auto sa = sockaddr();
    return sa.ip() + ':' + panda::to_string(sa.port());
}

NetLoc TServer::netloc () const {
    return { sockaddr().ip(), sockaddr().port() };
}

void TClient::request (const RequestSP& req) {
    if (sa) {
        req->uri->host(sa.ip());
        req->uri->port(sa.port());
    }
    if (secure) req->uri->scheme("https");
    Client::request(req);
}

ResponseSP TClient::get_response (const RequestSP& req) {
    ResponseSP response;

    req->response_event.add([this, &response](auto, auto& res, auto& err){
        if (err) throw err;
        response = res;
        loop()->stop();
    });

    request(req);
    loop()->run();

    return response;
}

ResponseSP TClient::get_response (const string& uri, Header&& headers, Body&& body, bool chunked) {
    auto b = Request::Builder().uri(uri).headers(std::move(headers)).body(std::move(body));
    if (chunked) b.chunked();
    return get_response(b.build());
}

std::error_code TClient::get_error (const RequestSP& req) {
    std::error_code error;

    req->response_event.add([this, &error](auto, auto, auto& err){
        error = err;
        loop()->stop();
    });

    request(req);
    loop()->run();

    return error;
}

ClientPair::ClientPair (const LoopSP& loop) {
    server = make_server(loop, {});
    client = new TClient(loop);
    client->sa = server->sockaddr();
}

ServerPair::ServerPair (const LoopSP& loop, Server::Config cfg) {
    server = make_server(loop, cfg);

    conn = new Tcp(loop);
    if (secure) conn->use_ssl();
    conn->connect_event.add([](auto& conn, auto& err, auto){ if (err) throw err; conn->loop()->stop(); });
    conn->connect(server->listeners().front()->sockaddr());
    loop->run();
}

RawResponseSP ServerPair::get_response () {
    if (!response_queue.size()) {
        conn->read_event.remove_all();
        conn->eof_event.remove_all();

        conn->read_event.add([&, this](auto, auto& str, auto& err) {
            if (err) throw err;
            while (str) {
                if (!parser.request()) {
                    if (source_request) parser.set_request(source_request);
                    else                parser.set_request(new RawRequest(Request::Method::GET, new URI("/")));
                }
                auto result = parser.parse_shift(str);
                if (result.error) {
                    WARN(result.error);
                    throw result.error;
                }
                if (result.state != State::done) return;
                response_queue.push_back(result.response);
            }
            if (response_queue.size()) conn->loop()->stop();
        });
        conn->eof_event.add([&, this](auto){
            eof = true;
            auto result = parser.eof();
            if (result.error) throw result.error;
            if (result.response) response_queue.push_back(result.response);
            conn->loop()->stop();
        });
        conn->loop()->run();

        conn->read_event.remove_all();
        conn->eof_event.remove_all();
    }

    if (!response_queue.size()) throw "no response";
    auto ret = response_queue.front();
    response_queue.pop_front();
    return ret;
}

bool ServerPair::wait_eof (int tmt) {
    if (eof) return eof;

    TimerSP timer;
    if (tmt) timer = Timer::once(tmt, [this](auto) {
        conn->loop()->stop();
    }, conn->loop());

    conn->eof_event.add([this](auto){
        eof = true;
        conn->loop()->stop();
    });

    conn->loop()->run();
    return eof;
}

//iptr<protocol::http::Request> parse_request(const string& buf) {
//    auto parser = make_iptr<protocol::http::RequestParser>();
//    protocol::http::RequestParser::Result result = parser->parse_first(buf);
//    //panda_log_debug("parse_request state [" << static_cast<int>(result.state) << "]"
////<< " " << result.position);
//    if(!result.state) {
//        throw std::runtime_error("request parser failed");
//    }
//
//    if(!result.request->is_valid()) {
//        throw std::runtime_error("request parser failed, invalid message");
//    }
//
//    return result.request;
//}
//
//// Send in request end get it back in reponse body
//std::tuple<protocol::http::ResponseSP, protocol::http::RequestSP> echo_request(iptr<uri::URI> uri,
//        protocol::http::Request::Method request_method,
//        const string& body,
//        protocol::http::Header header) {
//    //panda_log_debug("echo request");
//
//    client::Request::Builder builder = client::Request::Builder()
//        .method(request_method)
//        .uri(uri)
//        .timeout(100);
//
//    if(!header.empty()) {
//        builder.header(std::move(header));
//    }
//
//    if(!body.empty()) {
//        builder.body(body);
//    }
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
//    wait(200, Loop::default_loop());
//
//    if(response && response->is_valid()) {
//        auto echo_request = parse_request(response->body->as_buffer());
//        return std::make_tuple(response, echo_request);
//    } else {
//        return std::make_tuple(nullptr, nullptr);
//    }
//}
//
//std::tuple<server::ServerSP, uint16_t> echo_server(const string& name) {
//
//    auto server = make_iptr<server::Server>();
//    server->request_callback.add([&](server::ConnectionSP connection, protocol::http::RequestSP request, ResponseSP& response) {
//        std::stringstream ss;
//        ss << request;
//        string request_str(ss.str().c_str());
//
//        //panda_log_info("on_request, echoing back: [" << request_str << "]");
//
//        response.reset(Response::Builder()
//            .header(protocol::http::Header().date(connection->server()->http_date_now()))
//            .code(200)
//            .reason("OK")
//            .body(request_str)
//            .build());
//    });
//
//    server::Server::Config config;
//    config.locations.emplace_back(server::Location{"127.0.0.1"});
//    config.dump_file_prefix = "echo." + name;
//    server->configure(config);
//    server->run();
//
//    return std::make_tuple(server, server->listeners[0]->sockaddr().port());
//}
//
//std::tuple<server::ServerSP, uint16_t> redirect_server(const string& name, uint16_t to_port) {
//    string location = "http://127.0.0.1:" + to_string(to_port);
//    auto server = make_iptr<server::Server>();
//    server->request_callback.add([location, server](server::ConnectionSP /* connection */,
//                protocol::http::RequestSP /* request */,
//                ResponseSP& response) {
//        //panda_log_debug("request_callback: " << request);
//        response.reset(ResponseSP(Response::Builder()
//            .header(protocol::http::Header().date(server->http_date_now()).location(location))
//            .code(301)
//            .reason("Moved Permanently")
//            .body("<html><head><title>Moved</title></head><body><h1>Moved</h1><p>This page has moved to <a href=\""
//                + location + "\">" + location + "</a>.</p></body></html>",
//                "text/html")
//            .build()));
//    });
//
//    server::Server::Config config;
//    config.locations.emplace_back(server::Location{"127.0.0.1"});
//    config.dump_file_prefix = "redirect." + name;
//    server->configure(config);
//    server->run();
//
//    return std::make_tuple(server, server->listeners[0]->sockaddr().port());
//}
