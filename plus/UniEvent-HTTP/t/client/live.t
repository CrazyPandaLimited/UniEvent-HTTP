use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

plan skip_all => "set TEST_FULL to test requests to live servers" unless $ENV{TEST_FULL};

variate_catch('[client-live]', 'ssl');

done_testing();
