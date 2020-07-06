#!/usr/bin/perl

# Marhaba Bot ##########################################################

# Documentation ########################################################
## XML::Feed
## http://search.cpan.org/~davecross/XML-Feed/
## Reading and writing RSS and Atom with perl
## http://www.indecorous.com/perl/rss/
## Simple RSS with Perl
## http://www.theperlreview.com/rss.html
########################################################################

# Modules ##############################################################
use strict;
use warnings;
use LWP;
use XML::Feed;
use POSIX qw/strftime/;
use utf8;
use Text::Trim qw(trim);
#use Encode; # useful for UTF-8?
########################################################################

# Configuration ########################################################
## List of URIs
my $file = 'input.csv';
## Fake HTTP User-Agent
my @headers = (
   'User-Agent' => 'Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0',
   'Accept' => '*/*',
   'Accept-Charset' => 'utf-8'
);
## HTTP timeout
my $timeout = 1;
## XML Parser
## Default: XML:RSS
## LibXML
# $XML::Feed::Format::RSS::PREFERRED_PARSER = "XML::RSS::LibXML";
########################################################################

# Log ##################################################################
sub botLog {
    my $string = shift;
    my $date = strftime "%Y-%m-%d %k:%M:%S", localtime;
    open (LOG, '>>bot.log');
    print LOG "$date $string";
    close (LOG);
}
########################################################################

# Feed parser ##########################################################
## Example using XML::RAI
# print $r->content_type, "\n";
# print substr($xml, 0, 300);
# my $rai = XML::RAI->parse_string( $xml );
# print $rai->channel->title."\n\n";
# foreach my $item ( @{$rai->items} ) {
#  print $item->title."\n";
#  print $item->link."\n";
#  print $item->content."\n";
# print $item->issued."\n\n";
# }
# for ( @{$rai->items} ) {
#  print "Item:\n";
#  print "  Title: " . $_->title . "\n";
#  print "  Link: " . $_->link . "\n";
#  print "  Description: " . $_->description . "\n";
#  print "  Created: " . $_->created . "\n";
# }

sub feedParser {
    my $url = shift;
    my $feed;
    my $feedTitle;
    my $feedFormat;

    eval {
        $feed = XML::Feed->parse( URI->new($url) )
        or die XML::Feed->errstr;
    };
    if ( $@ ) {
        botLog( "XML PARSER ERROR\n" );
    } else {
        $feedTitle = $feed->title;
        $feedFormat = $feed->format;
        my $title = $feedTitle =~ s/\n//r;
        trim $title;    
        
        if ($title !~ qr/comment/miup and $url !~ qr/comment/miup) {
					botLog( "SOURCE $title ($feedFormat)\n" );

					open(RSS, '>>output.csv');
					print RSS "$url,$title,$feedFormat\n";
					close(RSS);
				}
    }
}
########################################################################

# URI ##################################################################
my $list;
eval {
    open $list, $file or die;
};
if ( $@ ) {
    botLog( "LIST $file ($!)\n" );
} else {
    my $c = 0;

    # Fake web browser #####################################################
    my $browser = LWP::UserAgent->new;
    $browser->timeout($timeout);

    while( my $url = <$list>)  {

        botLog( "URI $url" );

        my $response;
        eval {
            $response = $browser->get( $url, @headers );
            die $response->status_line unless $response->is_success;
        };
        if ( $@ ) {
            botLog( "WEB BROWSER ERROR\n" );
        } else {

            # HTML parser ##############################################
            ## Find all of the syndication feeds on a given page, using
            ## auto-discovery.
            my @links = XML::Feed->find_feeds( $url, @headers );

            ## Example using HTML::SimpleLinkExtor
            # use HTML::SimpleLinkExtor 1.27;
            ## Create SimpleLinkExtor object
            # my $linkparser = HTML::SimpleLinkExtor->new;
            ## Parse HTML response
            # $linkparser->parse( $response->content );
            ## Get a list of links
            # my @all_links = $linkparser->links;
            ############################################################

            while( my ( $k, $v ) = each @links ) {

                botLog( "FEED $v\n" );
                feedParser( $v );

                ## Example using LWP
                # my $b = LWP::UserAgent->new;
                # my $r = $b->get( $value, @headers );
                # if ( $r->content_type =~ m/xml/i ) {
                #  print $r->content_type, "\n";
                #  print substr($r->content, 0, 100);
                # }
            }
        }
        #last if $c == 179;
        $c += 1;
    }
    ####################################################################

    close $list;
}
########################################################################

