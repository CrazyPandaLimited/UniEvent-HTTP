#include <xs/unievent.h>
#include <xs/unievent/http.h>
#include "test.h"

using namespace panda;
using namespace panda::unievent::http;

namespace xs {

template <> struct Typemap<ServerPair*> : TypemapObject<ServerPair*, ServerPair*, ObjectTypePtr, ObjectStorageMG> {
    static string_view package () { return "MyTest::ServerPair"; }
};

}

MODULE = MyTest                PACKAGE = MyTest
PROTOTYPES: DISABLE

bool variate_ssl (SV* val = nullptr) {
    if (val) secure = SvTRUE(val);
    RETVAL = secure;
}

ServerPair* make_server_pair (LoopSP loop = Loop::default_loop()) {
    RETVAL = new ServerPair(loop);
}

MODULE = MyTest    PACKAGE = MyTest::ServerPair
    
#Server* ServerPair::server () {
#    RETVAL = THIS->server;
#}

Tcp* ServerPair::conn () {
    RETVAL = THIS->conn;
}

panda::protocol::http::Response* ServerPair::get_response (string s = {}) {
    if (s) RETVAL = THIS->get_response(s);
    else   RETVAL = THIS->get_response();
}

#void ServerPair::autorespond (ServerResponseSP res)

bool ServerPair::wait_eof (int tmt = 0)