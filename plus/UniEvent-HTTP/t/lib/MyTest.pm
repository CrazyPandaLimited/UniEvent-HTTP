package MyTest;
use 5.012;
use warnings;
use Test::More;
use Test::Catch;
use UniEvent::HTTP;
use Panda::Lib::Logger;

XS::Loader::load();

if (my $l = $ENV{LOG}) {
    set_native_logger(sub {
        my ($level, $cp, $msg) = @_;
        say "$cp $msg";
    });
    if ($l eq 'VD') { $l = LOG_VERBOSE_DEBUG }
    else            { $l = LOG_DEBUG }
    set_log_level($l);
}

$SIG{PIPE} = 'IGNORE';

sub import {
    my ($class) = @_;

    my $caller = caller();
    foreach my $sym_name (qw/variate_catch/) {
        no strict 'refs';
        *{"${caller}::$sym_name"} = \&{$sym_name};
    }
}

sub variate {
    my $sub = pop;
    my @names = reverse @_ or return;
    
    state $valvars = {
        ssl => [0,1],
    };
    
    my ($code, $end) = ('') x 2;
    $code .= "foreach my \$${_}_val (\@{\$valvars->{$_}}) {\n" for @names;
    $code .= "variate_$_(\$${_}_val);\n" for @names;
    my $stname = 'variation '.join(', ', map {"$_=\$${_}_val"} @names);
    $code .= qq#subtest "$stname" => \$sub;\n#;
    $code .= "}" x @names;
    
    eval $code;
    die $@ if $@;
}

sub variate_catch {
    my ($catch_name, @names) = @_;
    variate(@names, sub {
        my $add = '';
        foreach my $name (@names) {
            $add .= "[v-$name]" if MyTest->can("variate_$name")->();
        }
        catch_run($catch_name.$add);
    });
}


1;
