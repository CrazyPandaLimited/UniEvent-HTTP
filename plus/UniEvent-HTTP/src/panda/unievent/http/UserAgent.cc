#include "UserAgent.h"

namespace panda { namespace unievent { namespace http {


using namespace panda::protocol::http;

UserAgent::UserAgent(const LoopSP& loop, const string& serialized, const Config& config):
    _pool{Pool::instance(loop)}, _cookie_jar(new CookieJar(serialized)), _config(config) {

}

ClientSP UserAgent::request (const RequestSP& req) {
    if (!req->headers.has("User-Agent")) req->headers.add("User-Agent", _config.identity);
    req->response_event.add([&](auto& req, auto& res, auto& err){
        if (!err) _cookie_jar->collect(*res, req->uri);
    });
    req->redirect_event.add([&](auto& req, auto& res, auto& redirect_ctx){
        _cookie_jar->collect(*res, redirect_ctx->uri);
        _cookie_jar->populate(*req);
    });

    _cookie_jar->populate(*req);
    return _pool->request(req);
}


}}}
