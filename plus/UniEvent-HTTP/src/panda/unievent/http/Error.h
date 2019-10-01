#pragma once
#include <system_error>
#include <panda/exception.h>

namespace panda { namespace unievent { namespace http {

enum class errc {
    parser_error = 1
};

struct ErrorCategory : std::error_category {
    const char* name () const throw() override;
    std::string message (int condition) const throw() override;
};
extern ErrorCategory error_category;

std::error_code make_error_code (errc code) { return std::error_code((int)code, error_category); }

struct HttpError : panda::exception {};

}}}
