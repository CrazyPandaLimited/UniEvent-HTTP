use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[client-redirect]', 'ssl');

done_testing();