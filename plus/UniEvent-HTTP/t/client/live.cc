#include "../lib/test.h"

TEST_CASE("get real sites", "[client-live]") {
    auto l = Loop::default_loop();
    std::vector<string> sites = {
        "http://google.com",
        "http://youtube.com",
        "http://facebook.com",
        "http://wikipedia.org",
        "https://yandex.ru",
        "http://rbc.ru",
        "http://ya.ru",
        "http://example.com"
    };

    std::vector<ResponseSP> resps;

    for (auto site : sites) {
        panda_log_debug("request to: " << site);
        RequestSP req = Request::Builder()
            .uri(site)
            .response_callback([&](auto&, auto& res, auto& err) {
                panda_log_debug("GOT response " << res << ", " << res->body.length() << " bytes, err=" << err);
                CHECK(!err);
                resps.push_back(res);
            })
            .timeout(5000)
            .build();
        http_request(req, l);
    }

    l->run();

    CHECK(resps.size() == sites.size());
    for (auto res : resps) {
        CHECK(res->code == 200);
    }
}
