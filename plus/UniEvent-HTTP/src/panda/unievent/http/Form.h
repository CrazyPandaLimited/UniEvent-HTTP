#pragma once

#include <panda/string.h>
#include <panda/error.h>
#include <panda/protocol/http/Request.h>
#include <panda/unievent/Streamer.h>
#include <panda/unievent/streamer/Stream.h>

namespace panda { namespace unievent { namespace http {

namespace proto = protocol::http;
struct Client;
using Chunk = proto::Request::wrapped_chunk;

struct IFormItem: panda::Refcnt {
    string name;

    IFormItem(const string& name_) noexcept: name{name_}{};

    virtual bool start(proto::Request& req, Client& out) noexcept = 0;
    virtual void stop() noexcept {}
protected:
    void produce(const Chunk& chunk, Client& out) noexcept;
};
using FormItemSP = iptr<IFormItem>;

struct FormField: IFormItem {
    string content;

    inline FormField(const string& name_, const string& content_) noexcept: IFormItem(name_), content{content_} {}

    bool start(proto::Request& req, Client& out) noexcept override;
};

struct FormEmbeddedFile: FormField {
    string mime_type;
    string filename;

    FormEmbeddedFile(const string& name_, const string& content_, const string& mime_ = "application/octet-stream", const string& filename_ = "") noexcept:
        FormField(name_, content_), mime_type{mime_}, filename{filename_} {}

    bool start(proto::Request& req, Client& out) noexcept override;
};

struct FormFile: IFormItem {
    Streamer::IInputSP in;
    string mime_type;
    string filename;
    StreamerSP streamer;
    size_t max_buf;

    struct ClientOutput: streamer::StreamOutput {
        proto::RequestSP req;

        using streamer::StreamOutput::StreamOutput;
        ErrorCode write (const string& data) override;
    };

    FormFile(const string& name_, Streamer::IInputSP in_, const string& mime_, const string& filename_, size_t max_buf_ = 10000000) noexcept:
        IFormItem(name_), in{in_}, mime_type{mime_}, filename{filename_}, max_buf{max_buf_}  {
    }

    bool start(proto::Request& req, Client& out) noexcept override;
    void stop() noexcept override;
};


}}}
