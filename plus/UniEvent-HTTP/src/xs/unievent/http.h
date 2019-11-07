#pragma once
#include <xs/function.h>
#include <xs/protocol/http.h>
#include <panda/unievent/http.h>

namespace xs { namespace unievent { namespace http {

using namespace panda::unievent::http;

void fill_request  (const Hash&, Request*);
void fill_response (const Hash&, Response*);

}}}

namespace xs {

template <class TYPE>
struct Typemap<panda::unievent::http::Request*, TYPE> : Typemap<panda::protocol::http::Request*, TYPE> {
    static panda::string_view package () { return "UniEvent::HTTP::Request"; }
};

template <class TYPE>
struct Typemap<panda::unievent::http::RequestSP, panda::iptr<TYPE>> : Typemap<TYPE*> {
    using Super = Typemap<TYPE*>;
    static panda::iptr<TYPE> in (Sv arg) {
        if (!arg.is_hash_ref()) return Super::in(arg);
        panda::iptr<TYPE> ret = make_backref<TYPE>();
        xs::unievent::http::fill_request(arg, ret.get());
        return ret;
    }
};

template <class TYPE>
struct Typemap<panda::unievent::http::Response*, TYPE> : Typemap<panda::protocol::http::Response*, TYPE> {
    static panda::string_view package () { return "UniEvent::HTTP::Response"; }
};

template <class TYPE>
struct Typemap<panda::unievent::http::ResponseSP, panda::iptr<TYPE>> : Typemap<TYPE*> {
    using Super = Typemap<TYPE*>;
    static panda::iptr<TYPE> in (Sv arg) {
        if (!arg.is_hash_ref()) return Super::in(arg);
        panda::iptr<TYPE> ret = make_backref<TYPE>();
        xs::unievent::http::fill_response(arg, ret.get());
        return ret;
    }
};

template <class TYPE>
struct Typemap<panda::unievent::http::Pool*, TYPE> : TypemapObject<panda::unievent::http::Pool*, TYPE, ObjectTypeRefcntPtr, ObjectStorageMG> {
    static panda::string_view package () { return "UniEvent::HTTP::Pool"; }
};

}
