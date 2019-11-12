use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;
use Protocol::HTTP::Message;
use Protocol::HTTP::Request;

#variate_catch('[server-basic]', 'ssl');

subtest 'simple request' => sub {
    my $test = new UE::Test::Async(1);
    my $p    = new MyTest::ServerPair($test->loop);
    
    $p->server->request_callback(sub {
        my $req = shift;
        $test->happens;
        is $req->method, METHOD_POST;
        is $req->state, STATE_DONE;
        is $req->body, "epta nah";
        $test->loop->stop;
    });
    
    $p->conn->write(
        "POST / HTTP/1.1\r\n".
        "Content-length: 8\r\n".
        "\r\n".
        "epta nah"
    );
    
    $test->run;
};

#subtest 'backref' => sub {
#    
#};

done_testing();
