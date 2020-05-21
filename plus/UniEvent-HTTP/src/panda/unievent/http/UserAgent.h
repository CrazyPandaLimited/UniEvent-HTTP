#pragma once

#include "panda/uri.h"
#include "panda/refcnt.h"
#include "Pool.h"
#include "panda/protocol/http/CookieJar.h"

namespace panda { namespace unievent { namespace http {

extern const string DEFAULT_UA;

struct UserAgent: Refcnt {
    struct Config {
        string ca;
        string cert;
        string key;
        URISP proxy;
        string identity = DEFAULT_UA;

        Config() {};
    };
    using CookieJarSP = protocol::http::CookieJarSP;

    UserAgent(const LoopSP& loop, const string& serialized = {}, const Config& config = Config());

    ClientSP request (const RequestSP& req);

    void cookie_jar(CookieJarSP& value)  noexcept { _cookie_jar = value;      }
    void identity  (const string& value) noexcept { _config.identity = value; }
    void ca        (const string& value) noexcept { _config.ca = value;       }
    void cert      (const string& value) noexcept { _config.cert = value;     }
    void key       (const string& value) noexcept { _config.key  = value;     }
    void proxy     (const URISP& value)  noexcept { _config.proxy = value;    }

    const CookieJarSP& cookie_jar() const noexcept { return _cookie_jar;      }
    const string&      identity()   const noexcept { return _config.identity; }
    const string&      ca()         const noexcept { return _config.ca;       }
    const string&      cert()       const noexcept { return _config.cert;     }
    const string&      key()        const noexcept { return _config.key;      }
    const URISP&       proxy()      const noexcept { return _config.proxy;    }

private:
    PoolSP _pool;
    CookieJarSP _cookie_jar;
    Config _config;
};

}}}
