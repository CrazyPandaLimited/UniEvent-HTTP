#include "Form.h"
#include "Request.h"
#include "Client.h"

namespace panda { namespace unievent { namespace http {


void IFormField::produce(const Chunk &chunk, Client& out) noexcept {
    out.send_chunk(chunk);
}

bool FormField::start(proto::Request &req, Client &out) noexcept {
    produce(req.form_field(name, content), out);
    return true;
}

bool FormEmbeddedFile::start(proto::Request &req, Client &out) noexcept {
    produce(req.form_file(name, filename, mime_type), out);
    produce(req.form_data(content), out);
    return true;
}


bool FormFile::start(proto::Request& req, Client& out) noexcept {
    Streamer::IOutputSP out_stream = new ClientOutput(ClientSP(&out));
    streamer = new Streamer(std::move(in), out_stream, max_buf, out.loop());
    streamer->start();
    produce(req.form_file(name, filename, mime_type), out);
    return false;
}

void FormFile::stop() noexcept {
    streamer->stop();
    streamer.reset();
}

ErrorCode FormFile::ClientOutput::write (const string& data) {
    auto chunk = req->form_data(data);
    stream->write(chunk.begin(), chunk.end());
    return {};
}

void FormFile::ClientOutput::stop () {
    req.reset();
    auto client = dynamic_pointer_cast<Client>(stream);
    client->form_file_compete(ErrorCode());
}




}}}
