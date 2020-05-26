use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

#variate_catch('[user-agent]', 'ssl');

subtest "simple UA usage" => sub {
    my $test = UE::Test::Async->new;
    my $srv  = MyTest::make_server($test->loop);

    $srv->request_event->add(sub {
        my $req = shift;
        is $req->header('User-Agent'), 'test-ua';
        $req->respond(UE::HTTP::ServerResponse->new({code => 200, cookies => { sid => { value => '123' }}}));
    });
    my $uri = URI::XS->new($srv->uri);


    my $ua = MyTest::TUserAgent->new($test->loop);
    $ua->identity('test-ua');
    my $req = UniEvent::HTTP::Request->new({ uri  => $uri });

    is scalar(keys %{ $ua->cookie_jar->all_cookies }), 0;
    my $c = $ua->request($req);
    my $res = $c->await_response($req);
    is $res->code, 200;
    is scalar(keys %{ $ua->cookie_jar->all_cookies }), 1;

    my $ua2 = MyTest::TUserAgent->new($test->loop, {serialized => $ua->to_string(1)});
    is_deeply $ua2->cookie_jar->to_string(1), $ua->to_string(1);
};

done_testing;
