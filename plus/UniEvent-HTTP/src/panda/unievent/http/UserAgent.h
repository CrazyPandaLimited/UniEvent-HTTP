#pragma once

#include "panda/uri.h"
#include "panda/refcnt.h"
#include "Pool.h"
#include "panda/protocol/http/CookieJar.h"


namespace panda { namespace unievent { namespace http {

struct UserAgent: Refcnt {
    struct Config {
        string identity = DEFAULT_UA;
        SSL_CTX* ssl_ctx = nullptr;
        URISP proxy;
        bool lax_context = false;

        Config() {};
    };
    using CookieJarSP = protocol::http::CookieJarSP;
    using Date = protocol::http::CookieJar::Date;


    UserAgent(const LoopSP& loop, const string& serialized = {}, const Config& config = Config());

    ClientSP request (const RequestSP& req, const URISP& context_uri, bool top_level = true);
    ClientSP request (const RequestSP& req, bool top_level = true) { return request(req, req->uri, top_level); }

    void cookie_jar(CookieJarSP& value)  noexcept { _cookie_jar = value;      }
    void identity  (const string& value) noexcept { _config.identity = value; }
    void proxy     (const URISP& value)  noexcept { _config.proxy = value;    }
    void ssl_ctx   (SSL_CTX* value)      noexcept { _config.ssl_ctx = value;  }

    const CookieJarSP& cookie_jar() const noexcept { return _cookie_jar;      }
    const string&      identity()   const noexcept { return _config.identity; }
    const URISP&       proxy()      const noexcept { return _config.proxy;    }
    const SSL_CTX*     ssl_ctx()    const noexcept { return _config.ssl_ctx;  }
    const LoopSP&      loop()       const noexcept { return _pool->loop();    }

    string to_string(bool include_session = false) noexcept;

private:
    void inject(const RequestSP& req, const URISP& context_uri, bool top_level, const Date& now) noexcept;

    PoolSP _pool;
    CookieJarSP _cookie_jar;
    Config _config;
};

using UserAgentSP = iptr<UserAgent>;

}}}
