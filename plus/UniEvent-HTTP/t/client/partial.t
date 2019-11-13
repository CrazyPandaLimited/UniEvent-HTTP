use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;
use Protocol::HTTP::Message;
use Protocol::HTTP::Request;

variate_catch('[client-partial]', 'ssl');

subtest "chunked response receive" => sub {
    my $test = new UE::Test::Async(1);
    my $p    = new MyTest::ClientPair($test->loop);

    my $sres;
    $p->server->request_callback(sub {
        my $req = shift;
        $sres = new UE::HTTP::ServerResponse({code => 200, chunked => 1});
        $req->respond($sres);
    });

    my $count = 10;
    my $res = $p->client->get_response({
        uri => '/',
        partial_callback => sub {
            my ($req, $res, $err) = @_;
            die $err if $err;
            cmp_ok $res->state, '>=', STATE_GOT_HEADER;
    
            if ($count--) {
                $sres->send_chunk("a");
                return;
            }
    
            $sres->send_final_chunk;
    
            $req->partial_callback(sub {
                my (undef, $res, $err) = @_;
                die $err if $err;
                return unless $res->state == STATE_DONE;
                $test->happens;
                is $res->body, "aaaaaaaaaa";
            });
        },
    });
    
    is $res->code, 200;
    ok $res->chunked;
    is $res->state, STATE_DONE;
    is $res->body, "aaaaaaaaaa";
};

subtest "chunked request send" => sub {
    my $test = new UE::Test::Async(1);
    my $p    = new MyTest::ClientPair($test->loop);

    my $sres;
    my $req = new UE::HTTP::Request({
        uri     => '/',
        method  => METHOD_POST,
        chunked => 1,
    });

    my $count = 10;
    $p->server->route_callback(sub {
        my $sreq = shift;
        is $sreq->method, METHOD_POST;
        is $sreq->uri->path, "/";
        $sreq->enable_partial;

        $sreq->partial_callback(sub {
            my ($sreq, $err) = @_;
            die $err if $err;
            if (--$count) {
                $req->send_chunk("a");
                return;
            }
            $req->send_final_chunk;

            $sreq->partial_callback(sub {
                my ($sreq, $err) = @_;
                die $err if $err;
                return unless $sreq->state == STATE_DONE;
                ok $sreq->chunked;
                $test->happens;
                is $sreq->body, "aaaaaaaaaa";
                $sreq->respond(new UE::HTTP::ServerResponse({code => 200, body => $sreq->body}));
            });
        });

        $req->send_chunk("a");
    });

    my $res = $p->client->get_response($req);
    is $res->code, 200;
    ok !$res->chunked;
    is $res->body, "aaaaaaaaaa";
};

subtest "100-continue" => sub {
    my $test = new UE::Test::Async(3);
    my $p    = new MyTest::ClientPair($test->loop);

    $p->server->route_callback(sub {
        my $req = shift;
        $req->send_continue for 1..3;
    });
    $p->server->autorespond(new UE::HTTP::ServerResponse({code => 200}));

    my $res = $p->client->get_response({
        uri               => "/",
        headers           => {"Expect" => "100-continue"},
        continue_callback => sub { $test->happens },
    });
    is $res->code, 200;
};

done_testing();