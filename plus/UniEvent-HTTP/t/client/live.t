use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

plan skip_all => "set TEST_FULL to test requests to live servers" unless $ENV{TEST_FULL};

variate_catch('[client-live]', 'ssl');

subtest "default loop" => sub {
    my $loop = new UE::Loop->default_loop;
    my $test = new UE::Test::Async(1, 1, $loop);
    UE::HTTP::http_request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            is $response->code, 200;
            $test->happens;
            $loop->stop;
        },
    });
    $loop->run;
};

subtest "separate loop" => sub {
    my $loop = new UE::Loop->new;
    my $test = new UE::Test::Async(1, 1, $loop);
    UE::HTTP::http_request({
        uri => 'http://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            is $response->code, 200;
            $test->happens;
            $loop->stop;
        },
    }, $loop);
    $loop->run;
};

subtest "https" => sub {
    my $loop = new UE::Loop->default_loop;
    my $test = new UE::Test::Async(1, 1, $loop);
    UE::HTTP::http_request({
        uri => 'https://rbc.ru',
        response_callback => sub {
            my ($request, $response) = @_;
            is $response->code, 200;
            $test->happens;
            $loop->stop;
        },
    });
    $loop->run;
};

subtest "sync simple get" => sub {
    my ($body, $err) = UE::HTTP::http_get("https://ya.ru");
    is $err, undef;
    cmp_ok length($body), '>', 0, "body length = ".length($body);
};

done_testing();
