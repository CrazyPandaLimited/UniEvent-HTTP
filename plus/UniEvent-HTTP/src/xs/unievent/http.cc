#include "http.h"

namespace xs { namespace unievent { namespace http {

void fill_request (const Hash& p, Request* req) {
    xs::protocol::http::fill_request(p, req);
    Sv sv;
    if ((sv = p.fetch("response_callback"))) req->response_event.add(xs::in<Request::response_fn>(sv));
    if ((sv = p.fetch("redirect_callback"))) req->redirect_event.add(xs::in<Request::redirect_fn>(sv));
    Simple v;
    if ((v = p.fetch("timeout"))) req->timeout = (double)v * 1000;
    if ((v = p.fetch("redirection_limit"))) req->redirection_limit = v;
}

void fill_response (const Hash& p, Response* res) {
    xs::protocol::http::fill_response(p, res);
}

}}}
