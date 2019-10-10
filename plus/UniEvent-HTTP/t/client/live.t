use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

plan skip_all => "set TEST_LIVE to test requests to live servers" unless $ENV{TEST_LIVE};

Test::Catch::run('[client-live]');

done_testing();
