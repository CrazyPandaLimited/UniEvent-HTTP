#include "XSRequest.h"

#include <xs/uri.h>

using xs::my_perl;

namespace xs { namespace unievent { namespace http { namespace client {

void XSRequest::response_cb (RequestSP request, ResponseSP response) {
    static_cast<XSRequest*>(request.get())->xs_response_cb.call({xs::out(request), xs::out(response)});
}

void XSRequest::redirect_cb (RequestSP request, iptr<URI> uri) {
    static_cast<XSRequest*>(request.get())->xs_redirect_cb.call({xs::out(request), xs::out(uri.get())});
}

void XSRequest::error_cb (RequestSP request, const string& error) {
    static_cast<XSRequest*>(request.get())->xs_error_cb.call({xs::out(request), xs::out(error)});
}

}}}} // namespace xs::unievent::http::client
