package MyTest;
use 5.012;
use warnings;
use Test::More;
use Test::Catch;
use UniEvent::HTTP;
use Panda::Lib::Logger;
use Scalar::Util 'weaken';

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
    foreach my $sym_name (qw/variate_catch fail_cb/) {
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

sub fail_cb { fail("should not be called"); }

sub make_server {
    my ($loop, $cfg) = @_;
    my $server = new MyTest::TServer($loop);
    $cfg ||= {};
    $cfg->{locations} = [{host => "127.0.0.1"}];
    $server->configure($cfg);
    $server->run;
    return $server;
}

{
    package MyTest::TServer;
    use parent 'UniEvent::HTTP::Server';
    use 5.012;
    
    sub new {
        my $self = shift->SUPER::new(@_);
        XS::Framework::obj2hv($self);
        return $self;
    }
    
    sub autorespond {
        my ($self, $res) = @_;
        my $queue = $self->{autores_queue} ||= [];
        unless (!$self->{autores}++) {
            $self->request_event->add(sub {
                my $req = shift;
                return unless @$queue;
                $req->respond(shift @$queue);
            });
        }
        push @$queue, $res;
    }
}

{
    package MyTest::ServerPair;
    use 5.012;
    use Protocol::HTTP::Message;
    use Protocol::HTTP::Request;

    sub new {
        my ($class, $loop, $cfg) = @_;
        my $self = bless {}, $class;
        
        $self->{server} = MyTest::make_server($loop, $cfg);
        $self->{conn}   = new UE::Tcp($loop);
        
        $self->{conn}->connect_callback(sub {
            my ($conn, $err) = @_;
            die $err if $err;
            $conn->loop->stop();
        });
        
        $self->{conn}->connect_addr($self->{server}->sockaddr);
        $loop->run;
            
        return $self;
    }
    
    sub server {shift->{server}}
    sub conn   {shift->{conn}}
    
    sub get_response {
        my ($self, $send_str) = @_;
        my $conn = $self->{conn};
        $conn->write($send_str) if $send_str;
        
        my $queue = $self->{respone_queue} ||= [];
        unless (@$queue) {
            my $parser = $self->{parser} ||= new Protocol::HTTP::ResponseParser();
            my $source_request = $self->{source_request};
            my $eofref = \$self->{eof};
            $conn->read_callback(sub {
                my ($conn, $str, $err) = @_;
                die $err if $err;
                while ($str) {
                    unless ($parser->request) {
                        $source_request ? $parser->set_request($source_request) : $parser->set_request(new Protocol::HTTP::Request({method => METHOD_GET}));
                    }
                    my ($res, $state, $err) = $parser->parse_shift($str);
                    die $err if $err;
                    return unless $state == STATE_DONE;
                    push @$queue, $res;
                }
                $conn->loop->stop if @$queue;
            });
            $conn->eof_callback(sub {
                my $conn = shift;
                $$eofref = 1;
                my ($res, $state, $err) = $parser->eof;
                die $err if $err;
                push @$queue, $res if $res;
                $conn->loop->stop;
            });
            $conn->loop->run;
    
            $conn->read_event->remove_all;
            $conn->eof_event->remove_all;
        }
    
        die "no response" unless @$queue;
        return shift @$queue;
    }
    
    sub wait_eof {
        my ($self, $tmt) = @_;
        return $self->{eof} if $self->{eof};
        my $conn = $self->{conn};
    
        my $timer;
        $timer = UE::Timer->once($tmt, sub { $conn->loop->stop }, $conn->loop) if $tmt;
    
        $conn->eof_callback(sub {
            $self->{eof} = 1;
            $conn->loop->stop;
        });
    
        $conn->loop->run;
        $conn->eof_event->remove_all;
        return $self->{eof};
    }
}

1;
