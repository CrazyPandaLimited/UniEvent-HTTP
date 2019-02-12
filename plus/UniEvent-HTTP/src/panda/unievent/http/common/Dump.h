#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>

namespace panda { namespace unievent { namespace http {

inline
void dump_message(const string& prefix, uint64_t request_number, uint64_t part_number, const string& part) {
    std::stringstream ss;
    ss << prefix << "."
        << std::setw(3) << std::setfill('0') << request_number
        << "."
        << std::setw(3) << std::setfill('0')<< part_number << ".dump";
    std::ofstream file(ss.str(), std::ios::binary);
    file << part;
    _ETRACE("dumping message: %s", ss.str().c_str());
}

}}} // namespace panda::unievent::http
