use 5.012;
use lib 't/lib';
use MyTest;
use UniEvent::HTTP;
use Test::Catch '[http-client]';

sub test_live {
    my $loop = UniEvent::Loop->default_loop;

    UniEvent::HTTP::http_request(new UniEvent::HTTP::Request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            $loop->stop;
        },
    }));

    $loop->run;
}

#test_live();
