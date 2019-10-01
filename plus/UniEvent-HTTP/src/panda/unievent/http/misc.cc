#pragma once
#include "Error.h"

namespace panda { namespace unievent { namespace http {

ErrorCategory error_category;

const char* ErrorCategory::name () const throw() { return "unievent-http"; }

std::string ErrorCategory::message (int condition) const throw() {
    switch ((errc)condition) {
        case errc::parser_error : return "parser error";
    }
    return {};
}

}}}
