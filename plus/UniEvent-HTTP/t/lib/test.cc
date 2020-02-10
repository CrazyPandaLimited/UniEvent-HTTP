#include "test.h"
#include <openssl/err.h>
#include <openssl/dh.h>
#include <openssl/ssl.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <iostream>

using panda::unievent::Timer;
using panda::unievent::TimerSP;

bool secure = false;

string active_scheme() { return string(secure ? "https" : "http"); }


int TServer::dcnt;
int TClient::dcnt;

TServerSP make_server (const LoopSP& loop, Server::Config cfg) {
    TServerSP server = new TServer(loop);

    Server::Location loc;
    loc.host = "127.0.0.1";
    SslHolder ssl_ctx;
    if (secure) { ssl_ctx = TServer::get_context("ca"); loc.ssl_ctx = ssl_ctx.get(); }
    cfg.locations.push_back(loc);
    cfg.tcp_nodelay = true;
    server->configure(cfg);

    server->run();
    return server;
}

void TServer::enable_echo () {
    request_event.remove_all();
    request_event.add([](auto& req){
        auto h = std::move(req->headers);
        h.remove("Host");
        h.remove("User-Agent");
        h.remove("Accept-Encoding");
        h.remove("Content-Length"); /* causes problems if req is not compressed, and res is */
        auto res = new ServerResponse(200, std::move(h), Body(req->body.to_string()));
        res->compressed = compression::GZIP;
        req->respond(res);
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
    return { sockaddr().ip(), sockaddr().port(), nullptr, {} };
}

SslHolder TServer::get_context(string cert_name) {
    auto ctx = SSL_CTX_new(SSLv23_server_method());
    SslHolder r(ctx, [](auto* ctx){ SSL_CTX_free(ctx); });
    string path("t/cert");
    string cert = path + "/" + cert_name + ".pem";
    string key = path + "/" + cert_name + ".key";
    int err;

    err = SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM);
    assert(err);

    err = SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM);
    assert(err);

    err = SSL_CTX_check_private_key(ctx);
    assert(err);
    return r;
}

void TClient::request (const RequestSP& req) {
    req->tcp_nodelay = true;
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

ResponseSP TClient::get_response (const string& uri, Headers&& headers, Body&& body, bool chunked) {
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

std::error_code TClient::get_error (const string& uri, Headers&& headers, Body&& body, bool chunked) {
    auto b = Request::Builder().uri(uri).headers(std::move(headers)).body(std::move(body));
    if (chunked) b.chunked();
    return get_error(b.build());
}

SslHolder TClient::get_context(string cert_name, const string& ca_name) {
    auto ctx = SSL_CTX_new(SSLv23_client_method());
    SslHolder r(ctx, [](auto* ctx){ SSL_CTX_free(ctx); });
    string path("t/cert");
    string ca = path + "/" + ca_name + ".pem";
    string cert = path + "/" + cert_name + ".pem";
    string key = path + "/" + cert_name + ".key";
    int err;

    err = SSL_CTX_load_verify_locations(ctx, ca.c_str(), nullptr);
    assert(err);

    err = SSL_CTX_use_certificate_file(ctx, cert.c_str(), SSL_FILETYPE_PEM);
    assert(err);

    err = SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), SSL_FILETYPE_PEM);
    assert(err);

    SSL_CTX_check_private_key(ctx);
    assert(err);

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    SSL_CTX_set_verify_depth(ctx, 4);

    return r;
}


TClientSP TPool::request (const RequestSP& req) {
    TClientSP client = dynamic_pointer_cast<TClient>(Pool::request(req));
    return client;
}

std::vector<ResponseSP> TPool::await_responses(std::vector<RequestSP>& reqs) {
    std::vector<ResponseSP> r;
    for(auto& req: reqs) {
        req->response_event.add([&r,&reqs, this](auto, auto& res, auto& err){
            if (err) throw err;
            r.emplace_back(res);
            if (r.size() == reqs.size()) {
                loop()->stop();
            }
        });
    }
    loop()->run();
    return r;
}

ClientPair::ClientPair (const LoopSP& loop) {
    server = make_server(loop, {});
    client = new TClient(loop);
    client->sa = server->sockaddr();
}

ServerPair::ServerPair (const LoopSP& loop, Server::Config cfg) {
    server = make_server(loop, cfg);

    conn = new Tcp(loop);
    SslHolder ssl;
    if (secure) { ssl = TClient::get_context("01-alice"); conn->use_ssl(ssl.get()); }
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
                if (!parser.context_request()) {
                    if (source_request) parser.set_context_request(source_request);
                    else                parser.set_context_request(new RawRequest(Request::Method::GET, new URI("/")));
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
