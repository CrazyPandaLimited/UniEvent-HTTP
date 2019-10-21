#pragma once
#include "BasicResponse.h"

namespace panda { namespace unievent { namespace http {

struct ServerRequest;

struct ServerResponse : BasicResponse {
    struct Builder;

    ServerResponse () : _request(), _completed() {}

    ServerResponse (int code, const string& reason, Header&& header, Body&& body, HttpVersion http_version, bool chunked)
        : BasicResponse(code, reason, std::move(header), std::move(body), http_version, chunked), _request(), _completed()
    {}

    void send_chunk     (const string& chunk);
    void finalize_chunk ();

private:
    friend ServerRequest;
    friend struct ServerConnection;

    ServerRequest* _request;
    bool           _completed;

};
using ServerResponseSP = iptr<ServerResponse>;

struct ServerResponse::Builder : protocol::http::Response::BuilderImpl<Builder> {
    ServerResponseSP build () {
        return new ServerResponse(_code, _message, std::move(_headers), std::move(_body), _http_version, _chunked);
    }
};

}}}
