#include "UserAgent.h"
#include <openssl/ssl.h>

namespace panda {

void refcnt_inc (const SSL_CTX* o) { SSL_CTX_up_ref((SSL_CTX*)o); }
void refcnt_dec (const SSL_CTX* o) { SSL_CTX_free((SSL_CTX*)o); }

}

namespace panda { namespace unievent { namespace http {

using namespace panda::protocol::http;

UserAgent::UserAgent(const LoopSP& loop, const string& serialized, const Config& config):
    _pool{Pool::instance(loop)}, _cookie_jar(new CookieJar(serialized)), _config(config) {
    commit_ssl();
}

ClientSP UserAgent::request (const RequestSP& req,  const URISP& context_uri, bool top_level) {
    if (!req->headers.has("User-Agent")) req->headers.add("User-Agent", _config.identity);
    req->response_event.add([ua = UserAgentSP(this)](auto& req, auto& res, auto& err){
        if (!err) {
            auto now = Date(ua->loop()->now());
            ua->cookie_jar()->collect(*res, req->uri, now);
        }
    });
    req->redirect_event.add([ua = UserAgentSP(this), context_uri, top_level](auto& req, auto& res, auto& redirect_ctx){
        auto& jar = ua->cookie_jar();
        auto now = Date(ua->loop()->now());
        jar->collect(*res, redirect_ctx->uri, now);
        ua->inject(req, context_uri, top_level, now);
    });

    auto now = Date(loop()->now());
    inject(req, context_uri, top_level, now);
    return _pool->request(req);
}

void UserAgent::inject(const RequestSP& req, const URISP& context_uri, bool top_level, const Date& now) noexcept {
    _cookie_jar->populate(*req, context_uri, top_level, now);
    if (!req->headers.has("User-Agent")) req->headers.add("User-Agent", _config.identity);
    if (_sslsp && !req->ssl_ctx) req->ssl_ctx = _sslsp.get();
    if (_config.proxy && !req->proxy) req->proxy = _config.proxy;
}


void UserAgent::commit_ssl() {
    _sslsp.reset();
    if (_config.ca && _config.key && _config.cert) {
        auto ctx = SSL_CTX_new(SSLv23_client_method());
        SSLSP ssl;
        ssl.reset(ctx);

        int ok;

        ok = SSL_CTX_load_verify_locations(ctx, _config.ca.c_str(), nullptr);
        if (!ok) throw string("error loading ca at " + _config.ca);

        ok = SSL_CTX_use_certificate_file(ctx, _config.cert.c_str(), SSL_FILETYPE_PEM);
        if (!ok) throw string("error loading cert at " + _config.cert);

        ok = SSL_CTX_use_PrivateKey_file(ctx, _config.key.c_str(), SSL_FILETYPE_PEM);
        if (!ok) throw string("error loading key at " + _config.key);

        ok = SSL_CTX_check_private_key(ctx);
        if (!ok) throw string("private key does not pass validation");

        _sslsp = std::move(ssl);
    }
}

string UserAgent::to_string(bool include_session) noexcept {
    auto now = Date(loop()->now());
    return _cookie_jar->to_string(include_session, now);
}


}}}