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
        //"http://yandex.ru",
        "http://rbc.ru",
        "http://amazon.com"
    };

    for(auto site : sites) {
        panda_log_debug("request to: " << site);
        client::ResponseSP response;
        client::RequestSP request = client::Request::Builder()
            .method(protocol::http::Request::Method::GET)
            .uri(site)
            .header(protocol::http::Header::Builder()
                    .add_field("Cookie", "session-id-time=2082787201l; session-id=139-0015980-3028813712; skin=noskin")
                .build())
            .response_callback([&](client::RequestSP, client::ResponseSP r) {
                response = r;
                //panda_log_debug("response " << response);
            })
            .timeout(5000)
            .build();
        
        //panda_log_debug("request " << request);
        client::http_request(request);

        wait(5000, Loop::default_loop());

        //REQUIRE(response);
    }
}
