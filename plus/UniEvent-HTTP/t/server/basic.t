use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;

variate_catch('[server-basic]', 'ssl');
done_testing();
