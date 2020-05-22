#pragma once

struct ssl_ctx_st;    typedef ssl_ctx_st SSL_CTX;

namespace panda {

void refcnt_inc (const SSL_CTX* o);
void refcnt_dec (const SSL_CTX* o);

}

#include "panda/uri.h"
#include "panda/refcnt.h"
#include "Pool.h"
#include "panda/protocol/http/CookieJar.h"


namespace panda { namespace unievent { namespace http {

using SSLSP = iptr<SSL_CTX>;

struct UserAgent: Refcnt {
    struct Config {
        string identity = DEFAULT_UA;
        string ca;
        string cert;
        string key;
        URISP proxy;
        bool lax_context = false;

        Config() {};
    };
    using CookieJarSP = protocol::http::CookieJarSP;
    using Date = protocol::http::CookieJar::Date;


    UserAgent(const LoopSP& loop, const string& serialized = {}, const Config& config = Config());

    ClientSP request (const RequestSP& req);

    void cookie_jar(CookieJarSP& value)  noexcept { _cookie_jar = value;      }
    void identity  (const string& value) noexcept { _config.identity = value; }
    void proxy     (const URISP& value)  noexcept { _config.proxy = value;    }

    void ca        (const string& value) noexcept { _config.ca = value;   }
    void cert      (const string& value) noexcept { _config.cert = value; }
    void key       (const string& value) noexcept { _config.key = value;  }
    void commit_ssl();

    const CookieJarSP& cookie_jar() const noexcept { return _cookie_jar;      }
    const string&      identity()   const noexcept { return _config.identity; }
    const string&      ca()         const noexcept { return _config.ca;       }
    const string&      cert()       const noexcept { return _config.cert;     }
    const string&      key()        const noexcept { return _config.key;      }
    const URISP&       proxy()      const noexcept { return _config.proxy;    }
    const LoopSP&      loop()       const noexcept { return _pool->loop();    }

    string to_string(bool include_session = false) noexcept;

private:
    void inject(const RequestSP& req, const Date& now) noexcept;

    PoolSP _pool;
    CookieJarSP _cookie_jar;
    Config _config;
    SSLSP _sslsp;
};

using UserAgentSP = iptr<UserAgent>;

}}}
