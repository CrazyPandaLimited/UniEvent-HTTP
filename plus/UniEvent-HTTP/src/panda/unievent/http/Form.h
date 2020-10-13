#pragma once

#include <panda/string.h>
#include <panda/error.h>
#include <panda/protocol/http/Request.h>

namespace panda { namespace unievent { namespace http {

namespace proto = protocol::http;
struct Client;
using Chunk = proto::Request::wrapped_chunk;

struct OperationResult {
    const ErrorCode ec;
    bool finished;
};

struct IFormField: panda::Refcnt {
    string name;

    IFormField(const string& name_) noexcept: name{name_}{};

    void produce(const Chunk& chunk, Client& out) noexcept;
    virtual OperationResult produce(proto::Request& req, Client& out) noexcept = 0;
};
using FormFieldSP = iptr<IFormField>;

struct FormField: IFormField {
    string content;

    inline FormField(const string& name_, const string& content_) noexcept: IFormField(name_), content{content_} {}

    OperationResult produce(proto::Request& req, Client& out) noexcept override;
};

struct FormEmbeddedFile: FormField {
    string mime_type;
    string filename;

    FormEmbeddedFile(const string& name_, const string& content_, const string& mime_, const string& filename_) noexcept:
        FormField(name_, content_), mime_type{mime_}, filename{filename_} {}

    OperationResult produce(proto::Request& req, Client& out) noexcept override;
};


}}}
