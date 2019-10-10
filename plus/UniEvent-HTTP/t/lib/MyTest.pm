package MyTest;
use 5.012;
use warnings;
use UniEvent::HTTP;
use Test2::IPC;
use Test::Catch;
use Panda::Lib::Logger;

XS::Loader::load();

if (my $l = $ENV{LOG}) {
    say "EPTA";
    set_native_logger(sub {
        my ($level, $cp, $msg) = @_;
        say "$cp $msg";
    });
    if ($l eq 'VD') { $l = LOG_VERBOSE_DEBUG }
    else            { $l = LOG_DEBUG }
    set_log_level($l);
}

$SIG{PIPE} = 'IGNORE';

1;
