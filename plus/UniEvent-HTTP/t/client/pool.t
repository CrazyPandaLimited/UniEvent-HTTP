use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[client-pool]', 'ssl');

subtest "reusing connection" => sub {
    my $test = new UE::Test::Async();
    my $pool = new MyTest::TPool($test->loop);
    my $srv  = MyTest::make_server($test->loop);
    $srv->autorespond(new UE::HTTP::ServerResponse({code => 200}));
    $srv->autorespond(new UE::HTTP::ServerResponse({code => 200}));

    my $c = $pool->get($srv->sockaddr->ip, $srv->sockaddr->port);
    $c->{sa} = $srv->sockaddr;

    is $pool->size, 1;
    is $pool->nbusy, 1;

    my $res = $c->get_response({uri => "/"});
    is $res->code, 200;

    my $c2 = $pool->get($srv->sockaddr->ip, $srv->sockaddr->port);
    is $c, $c2;

    is $pool->size, 1;
    is $pool->nbusy, 1;

    $res = $c->get_response({uri => "/"});
    is $res->code, 200;
};

subtest 'http_request' => sub {
    my $test = new UE::Test::Async(1);
    my $srv = MyTest::make_server($test->loop);
    $srv->autorespond(new UE::HTTP::ServerResponse({code => 200, body => "hi"}));

    UE::HTTP::http_request({
        uri => "http://".$srv->location.'/',
        response_callback => sub {
            my (undef, $res, $err) = @_;
            ok !$err;
            is $res->body, "hi";
            $test->happens;
            $test->loop->stop;
        },
    }, $test->loop);

    my $pool = UE::HTTP::Pool::instance($test->loop);
    is $pool->size, 1;
    is $pool->nbusy, 1;

    $test->run;
};

done_testing();