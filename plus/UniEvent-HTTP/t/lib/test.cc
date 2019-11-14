#include "test.h"
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>

using panda::unievent::Timer;
using panda::unievent::TimerSP;

bool secure = false;

int TServer::dcnt;
int TClient::dcnt;

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
