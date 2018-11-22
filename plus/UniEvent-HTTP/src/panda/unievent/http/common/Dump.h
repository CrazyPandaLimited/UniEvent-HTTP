#pragma once

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>

#include <panda/log.h>

namespace panda { namespace unievent { namespace http {

inline
void dump_message(const string& prefix, uint64_t request_number, uint64_t part_number, const string& part) {
    std::stringstream ss;
    ss << prefix << "." 
        << std::setw(3) << std::setfill('0') << request_number 
        << "." 
        << std::setw(3) << std::setfill('0')<< part_number << ".dump";
    panda_log_debug("dumping message " << ss.str());
    std::ofstream file(ss.str(), std::ios::binary);
    file << part;
}

}}} // namespace panda::unievent::http
