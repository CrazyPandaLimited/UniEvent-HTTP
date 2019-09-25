#include "../lib/test.h"

TEST_CASE("get real sites", "[live]") {
    if(!check_internet_available()) {
        panda_log_debug("No internet available");
        return;
    }

    std::vector<string> sites = {
        //"http://google.com",  // TODO uncomment when unbanned
        "http://youtube.com",
        "http://facebook.com",
        "http://wikipedia.org",
        "http://yandex.ru",
        "http://rbc.ru",
        "http://amazon.com"
    };

    for(auto site : sites) {
        //panda_log_debug("request to: " << site);
        ResponseSP response;
        client::RequestSP request = client::Request::Builder()
            .method(protocol::http::Request::Method::GET)
            .uri(site)
            .header(protocol::http::Header().add_field("Cookie", "session-id-time=2082787201l; session-id=139-0015980-3028813712; skin=noskin"))
            .response_callback([&](client::RequestSP, ResponseSP r) {
                response = r;
                //panda_log_debug("response " << response);
            })
            .timeout(5000)
            .build();

        //panda_log_debug("request " << request);
        http_request(request);

        wait(5000, Loop::default_loop());

        //TODO: uncomment when ipv6 is working
        //REQUIRE(response);
    }
}
