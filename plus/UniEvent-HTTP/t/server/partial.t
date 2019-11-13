use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;
use Protocol::HTTP::Message;

variate_catch('[server-partial]', 'ssl');

subtest "request receive" => sub {
    my $test = new UE::Test::Async(5);
    my $p    = new MyTest::ServerPair($test->loop);

    $p->server->route_callback(sub {
        my $req = shift;
        $test->happens;
        $req->enable_partial;
        $req->receive_callback(\&fail_cb);
        $req->partial_callback(sub {
            my ($req, $err) = @_;
            $test->happens;
            ok !$err;
            my $body = $req->body;
            if (!$body) {
                is $req->state, STATE_IN_BODY;
                $p->conn->write("1");
            }
            elsif ($body == "1") {
                is $req->state, STATE_IN_BODY;
                $p->conn->write("2");
            }
            elsif ($body == "12") {
                is $req->state, STATE_IN_BODY;
                $p->conn->write("3");
            }
            elsif ($body == "123") {
                is $req->state, STATE_DONE;
                $req->respond(new UE::HTTP::ServerResponse({code => 200, body => "epta"}));
            }
        });
    });

    $p->conn->write(
        "GET / HTTP/1.1\r\n".
        "Host: epta.ru\r\n".
        "Content-Length: 3\r\n".
        "\r\n"
    );

    my $res = $p->get_response;
    is $res->code, 200;
    is $res->body, "epta";
};

subtest 'xs request has backref' => sub {
    my $test = new UE::Test::Async(4);
    my $p    = new MyTest::ServerPair($test->loop);
    
    my $sreq;

    $p->server->route_callback(sub {
        my $req = shift;
        $req->enable_partial;
        $req->partial_callback(sub {
            my ($req, $err) = @_;
            $test->happens;
            ok !$err, "no err";
            
            if (!$sreq) { $sreq = $req }
            else {
                is $sreq, $req, "backref ok";
            }

            if ($req->state == STATE_DONE) {
                $test->loop->stop;
            } else {
                $p->conn->write("a");
            }
        });
    });

    $p->conn->write(
        "GET / HTTP/1.1\r\n".
        "Host: epta.ru\r\n".
        "Content-Length: 3\r\n".
        "\r\n"
    );

    $test->run;
};

done_testing();
