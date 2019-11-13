use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[server-keep-alive]', 'ssl');

subtest "idle timeout" => sub {
    my $test = new UE::Test::Async();
    my $p    = new MyTest::ServerPair($test->loop, {idle_timeout => 0.01});
    ok !$p->wait_eof(0.005);
    ok $p->wait_eof(0.05);
};

done_testing();
