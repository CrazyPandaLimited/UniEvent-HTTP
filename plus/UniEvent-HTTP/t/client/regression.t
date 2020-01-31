use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;
use Test::Exception;

subtest 'r1' => sub {
    my $client = UniEvent::HTTP::Client->new;
    $client->request({
        uri => 'https://ya.ru',
        timeout => 1,
        response_callback => sub {
            my ($request, $response) = @_;
            pass();
            $request->send_chunk(""); # this will throw exception because request is not active
        },
    });
    dies_ok { $client->loop->run };
};

subtest 'r2' => sub {
    my $client = UniEvent::HTTP::Client->new;
    $client->request({
        uri => 'https://ya.ru',
        timeout => 1,
        response_callback => sub {
            my ($request, $response) = @_;
            undef $client;
            pass();
        },
    });
    $client->loop->run;
};

done_testing();