use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[client-pool]', 'ssl');

done_testing();