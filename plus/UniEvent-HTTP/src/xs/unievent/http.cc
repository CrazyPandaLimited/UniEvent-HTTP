#include "http.h"

namespace xs { namespace unievent { namespace http {

RequestSP make_request (const Hash& p, const RequestSP& req) {
    auto ret = req ? req : RequestSP(new Request());
    xs::protocol::http::make_request(p, ret);
    Sv sv;
    if ((sv = p.fetch("response_callback"))) req->response_event.add(xs::in<Request::response_fn>(sv));
    if ((sv = p.fetch("redirect_callback"))) req->redirect_event.add(xs::in<Request::redirect_fn>(sv));
    Simple v;
    if ((v = p.fetch("timeout"))) req->timeout = (double)v * 1000;
    if ((v = p.fetch("redirection_limit"))) req->redirection_limit = v;
    return ret;
}

ResponseSP make_response (const Hash& p, const ResponseSP& res) {
    auto ret = res ? res : ResponseSP(new Response());
    xs::protocol::http::make_response(p, ret);
    return ret;
}

}}}
