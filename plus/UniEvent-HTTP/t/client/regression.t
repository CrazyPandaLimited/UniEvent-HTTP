use 5.012;
use lib 't/lib';
use MyTest;
use Test::More;
use Test::Exception;

subtest 'r1' => sub {
    my $client = UniEvent::HTTP::Client->new;
    $client->request({
        uri => 'https://ya.ru',
        timeout => 1,
        response_callback => sub {
            my ($request, $response) = @_;
            pass();
            $request->send_chunk(""); # this will throw exception because request is not active
        },
    });
    dies_ok { $client->loop->run };
};

subtest 'r2' => sub {
    my $client = UniEvent::HTTP::Client->new;
    $client->request({
        uri => 'https://ya.ru',
        timeout => 1,
        response_callback => sub {
            my ($request, $response) = @_;
            undef $client;
            pass();
        },
    });
    $client->loop->run;
};

#subtest 'r3' => sub {
#    our @foo;
#    my $i = 0;
#    my $sock = UE::Tcp->new;
#    #$sock->use_ssl($ctx);
#    $sock->bind('*', 6669);
#    $sock->listen(sub {
#        my (undef, $cli) = @_;
#        push @foo, $cli;
#        $cli->read_callback(sub {
#            $cli->write("OK");
#            $cli->shutdown;
#            $cli->disconnect;
#            undef $cli;
#        });
#    });
#    
#    UE::HTTP::http_request({
#        uri => 'http://dev.crazypanda.ru:6669/',
#        timeout => 2,
#        redirection_limit => 100_000,
#        redirect_callback => sub {
#            #        $_[0]->cancel;
#            warn "R";
#        },
#        response_callback => sub {
#            my ($request, $response) = @_;
#            warn "A: $_[2]";
#            warn $response->to_string;
#            #undef $client;
#        },
#    });
#    
#    UE::Loop->default->run;
#};

done_testing();