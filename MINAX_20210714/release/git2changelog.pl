#!/usr/bin/perl -w
#
=head USAGE
    
    ./git2changelog.pl Release-3.0.1..Release-3.0.2

    ./git2changelog.pl Release-3.0.1 ../git ../docs

 For bweb ReleaseNotes, use
    FORBWEB=1 ./git2changelog.pl Release-3.0.1..Release-3.0.2

=cut

use strict;
use POSIX q/strftime/;
use Cwd;

my $d='';
my $cur;
my %elt;
my @all;
my @usea;
my $last_txt='';
my %bugs;
my $refs = shift || '';
my $for_bweb = $ENV{FORBWEB}?1:0;
my $root = getcwd();
my $dir = shift || ".";
my $version="X.Y.Z";
my $topic;

if ($refs =~ /^Release-[\d\.]+$/) {
    $refs = "$refs..HEAD";
}

do {
    chdir($dir);

    $topic = "";
    if ($dir =~ m:/?(\w+)$:) {
        $topic = $1;
    }
    if ($topic eq 'bacula' || $topic eq 'git') {
        $topic = "";
    }

    if ( -f "bacula/src/version.h") {
        open(FP, "bacula/src/version.h");
        while (my $l = <FP>) {
            if ($l =~ /#define VERSION "(.+)"/) {
                $version = $1;
                last;
            }
        }
        close(FP);
    }
    open(FP, "git log --no-merges --pretty=format:'%at: %s' $refs|") or die "Can't run git log $!";
    while (my $l = <FP>) {

        # remove non useful messages
        next if ($l =~ /(tweak|typo|cleanup|regress:|again|.gitignore|fix compilation|technotes)/ixs);
        next if ($l =~ /update (version|technotes|kernstodo|projects|releasenotes|version|home|release|todo|notes|changelog|tpl|configure)/i);

        next if ($l =~ /bacula-web:/);

        next if ($topic eq 'build' && $l !~ /rpms|debs/);

        if ($for_bweb) {
            next if ($l !~ /bweb/ixs);
            $l =~ s/bweb: *//ig;
        }

        # keep list of fixed bugs
        if ($l =~ /(#|MA)0*(\d+)/) {
            $bugs{$2}=1;
        }

        if ($l !~ /^\d+: \w+( \w+)?:/ && $topic) {
            if ($l =~ /^(\d+:) (.*)/) {
                $l = "$1 $topic: $2";
            }
        }

        # remove old commit format
        $l =~ s/^(\d+): (kes|ebl)  /$1: /;

        if ($l =~ /(\d+): (.+)/) {
            # use date as 01Jan70
            my $dnow = strftime('%d%b%y', localtime($1));
            my $cur = strftime('%Y%m%d', localtime($1));
            my $txt = $2;

            # avoid identical multiple commit message
            next if ($last_txt eq $txt);
            $last_txt = $txt;

            # We format the string on 79 caracters
            $txt =~ s/\s\s+/ /g;
            $txt =~ s/.{70,77} /$&\n  /g;

            if ($txt =~ /usea:/) {
                push @usea, " - $txt";
                next;
            }

            # if we are the same day, just add entry
            if ($dnow ne $d) {
                $d = $dnow;
                if (!exists $elt{$cur}) {
                    push @{$elt{$cur}}, "\n\n$dnow";
                }
            }
            push @{$elt{$cur}},  " - $txt";
            push @all, " - $txt";

        } else {
            print STDERR "invalid format: $l\n";
        }
    }
    chdir($root);
    $dir = shift;
} while ($dir);

close(FP);

print "        ChangeLog for Bacula Enterprise version $version\n";
foreach my $d (sort {$b <=> $a} keys %elt) {
    print join("\n", @{$elt{$d}});
}

print "\n\nBugs fixed/closed since last release:\n";
print join(" ", sort keys %bugs), "\n";

print "\n================================================================\n";

my $date = strftime('%d %B %Y', localtime());
print "\n\n\n";
print "Release $version $date\n\n$version is a <minor/major> bug fix release.\n\n";

foreach my $d (sort @all) {
    print "$d\n";
}

print "\n\nBugs fixed/closed since last release:\n";
print join(" ", sort keys %bugs), "\n";


if (@usea) {
    print "\n==================== USEA  =====================================\n";

    my $date = strftime('%d %B %Y', localtime());
    print "\n\n\n";
    print "Release $version $date\n\n$version is a <minor/major> bug fix release.\n\n";

    foreach my $d (sort @usea) {
        print "$d\n";
    }
}
