use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[client-redirect]', 'ssl');

subtest "basic redirect" => sub {
    my $test = new UE::Test::Async(["connect", "redirect"]);
    my $p    = new MyTest::ClientPair($test->loop);

    $p->server->request_callback(sub {
        my $req = shift;
        if ($req->uri->path eq "/") {
            $req->redirect("/index");
        } elsif ($req->uri->path eq "/index") {
            $req->respond(new UE::HTTP::ServerResponse({code => 200, headers => {h => $req->header("h")}, body => $req->body}));
        }
    });

    $p->client->connect_callback(sub { $test->happens("connect"); });
    
    my $req = new UE::HTTP::Request({
        uri     => "/",
        headers => {h => 'v'},
        body    => 'b',
        redirect_callback => sub {
            my ($req, $res, $uri) = @_;
            $test->happens("redirect");
            is $res->code, 302;
            is $res->header("location"), "/index";
            is $uri->path, "/index";
            is $req->uri->path, "/";
            is $req->original_uri, $req->uri;
        },
    });

    my $res = $p->client->get_response($req);
    is $res->code, 200;
    is $res->header("h"), "v";
    is $res->body, "b";
    is $req->uri->path, "/index";
    is $req->original_uri->path, "/";
};

subtest "do not follow redirections" => sub {
    my $test = new UE::Test::Async();
    my $p    = new MyTest::ClientPair($test->loop);

    $p->server->autorespond(new UE::HTTP::ServerResponse({code => 302, headers => {location => "http://ya.ru"}}));

    my $res = $p->client->get_response({
        uri             => "/",
        follow_redirect => 0,
    });
    is $res->code, 302;
    is $res->header("Location"), "http://ya.ru";
};

subtest "redirection limit" => sub {
    my $test = new UE::Test::Async();
    my $p    = new MyTest::ClientPair($test->loop);

    $p->server->autorespond(new UE::HTTP::ServerResponse({code => 302, headers => {location => "http://ya.ru"}}));

    my $err = $p->client->get_error({
        uri               => "/",
        redirection_limit => 0,
        redirect_callback => sub { fail("should not be called") },
    });
    is $err, UE::HTTP::Error::unexpected_redirect;
};

done_testing();