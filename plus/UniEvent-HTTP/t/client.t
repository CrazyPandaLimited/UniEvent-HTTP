use 5.012;
use lib 't/lib';
use MyTest;
use UniEvent::HTTP;
use Test::Catch '[http-client]';

sub test_live {
    #my $loop = new UniEvent::Loop->default_loop;
    my $loop = new UniEvent::Loop;
    my $pool = new UniEvent::HTTP::ClientConnectionPool($loop);

    UniEvent::HTTP::http_request(new UniEvent::HTTP::Request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            $pool->clear;
            $loop->stop;
        },
        loop => $loop,
    }), $pool);

    $loop->run;
}

#test_live();
