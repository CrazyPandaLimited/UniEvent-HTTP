use 5.012;
use lib 't/lib';
use MyTest;
use UniEvent::HTTP;
use Test::Catch '[http-client]';

sub test_live_default_loop {
    my $loop = new UniEvent::Loop->default_loop;
    UniEvent::HTTP::http_request(new UniEvent::HTTP::Request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            $loop->stop;
        },
    }));

    $loop->run;
}

sub test_live_separate_loop {
    my $loop = new UniEvent::Loop;
    my $pool = new UniEvent::HTTP::ClientConnectionPool($loop);

    UniEvent::HTTP::http_request(new UniEvent::HTTP::Request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            $pool->clear;
            $loop->stop;
        },
    }), $pool);

    $loop->run;
}

test_live_default_loop();
test_live_separate_loop();
