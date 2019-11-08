#include "../lib/test.h"

#define TEST(name) TEST_CASE("client-live: " name, "[client-live]" VSSL)

TEST("get real sites") {
    std::vector<string> sites = {
        "http://google.com",
        "http://youtube.com",
        "http://facebook.com",
        "http://wikipedia.org",
        "http://yandex.ru",
        "http://rbc.ru",
        "http://ya.ru",
        "http://example.com"
    };

    AsyncTest test(5000, sites.size());

    for (auto site : sites) {
        panda_log_debug("request to: " << site);
        RequestSP req = Request::Builder()
            .uri(site)
            .response_callback([&](auto&, auto& res, auto& err) {
                panda_log_debug("GOT response " << res << ", " << res->body.length() << " bytes, err=" << err);
                test.happens();
                CHECK_FALSE(err);
                CHECK(res->code == 200);
            })
            .build();
        http_request(req, test.loop);
    }

    test.run();
}
