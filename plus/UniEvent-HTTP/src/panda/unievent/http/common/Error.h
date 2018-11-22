#pragma once

#include <string>
#include <exception>

namespace panda { namespace unievent { namespace http {

struct HttpError : std::runtime_error {
    HttpError(const std::string& msg) : std::runtime_error(msg) {
    }
};

struct ServerError : HttpError {
    ServerError(const std::string& msg) : HttpError(msg) {
    }
};

struct BadArgument : HttpError {
    BadArgument(const std::string& msg) : HttpError(msg) {
    }
};

struct PoolError : HttpError {
    PoolError(const std::string& msg) : HttpError(msg) {
    }
};

struct ProgrammingError : HttpError {
    ProgrammingError(const std::string& msg) : HttpError(msg) {
    }
};

}}} // namespace panda::unievent::http
