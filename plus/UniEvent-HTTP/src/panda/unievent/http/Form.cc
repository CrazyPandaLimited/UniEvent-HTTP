#include "Form.h"
#include "Request.h"
#include "Client.h"

namespace panda { namespace unievent { namespace http {


void IFormField::produce(const Chunk &chunk, Client& out) noexcept {
    out.send_chunk(chunk);
}

OperationResult FormField::produce(proto::Request &req, Client &out) noexcept {
    IFormField::produce(req.form_field(name, content), out);
    return OperationResult{ErrorCode(), true};
}

OperationResult FormEmbeddedFile::produce(proto::Request &req, Client &out) noexcept {
    IFormField::produce(req.form_file(name, filename, mime_type), out);
    IFormField::produce(req.form_data(content), out);
    return OperationResult{ErrorCode(), true};
}

}}}
