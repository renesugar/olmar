#!/usr/bin/perl -w
# configure script for elsa

use strict 'subs';

# defaults
$BASE_FLAGS = "-Wall -Wno-deprecated -D__UNIX__";
$default_hash = "-DWILKERSON_GOLDSMITH_HASH";
@CCFLAGS = ();
@LDFLAGS = ("-Wall");
$debug = 0;
$use_dash_g = 1;
$allow_dash_O2 = 1;
$SMBASE = "../smbase";
$AST = "../ast";
$ELKHOUND = "../elkhound";
$USE_GNU = "yes";


sub usage {
  print(<<"EOF");
usage: ./configure [options]
options:
  -h:                print this message
  -debug,-nodebug:   enable/disable debugging options [disabled]
  -no-dash-g         disable -g
  -no-dash-O2        disable -O2
  -prof              enable profiling
  -devel             add options useful while developing
  -gnu=yes/no        enable or disable GNU extensions [enabled]
  -ccflag <arg>:     add <arg> to gcc command line
  -smbase=<dir>:     specify where the smbase library is [$SMBASE]
  -ast=<dir>:        specify where the ast system is [$AST]
  -elkhound=<dir>:   specify where the elkhound system is [$ELKHOUND]
  -useSerialNumbers: give serial numbers to some objects for debugging
EOF
}


# process command-line arguments
$originalArgs = join(' ', @ARGV);
while (@ARGV) {
  my $tmp;
  my $arg = $ARGV[0];
  shift @ARGV;

  # treat leading "--" uniformly with leading "-"
  $arg =~ s/^--/-/;

  if ($arg eq "-h" ||
      $arg eq "-help") {
    usage();
    exit(0);
  }

  # things that look like options to gcc should just
  # be added to CCFLAGS
  elsif ($arg =~ m/^(-W|-D|-O)/) {
    push @CCFLAGS, $arg;
  }
  elsif ($arg eq "-ccflag") {
    push @CCFLAGS, $ARGV[0];
    shift @ARGV;
  }

  elsif ($arg eq "-d" ||
         $arg eq "-debug") {
    $debug = 1;
  }
  elsif ($arg eq "-nodebug") {
    $debug = 0;
  }
  elsif ($arg eq "-no-dash-g") {
    $use_dash_g = 0;
  }
  elsif ($arg eq "-no-dash-O2") {
    $allow_dash_O2 = 0;
  }

  elsif ($arg eq "-prof") {
    push @CCFLAGS, "-pg";
    push @LDFLAGS, "-pg";
  }

  elsif ($arg eq "-devel") {
    push @CCFLAGS, "-Werror";
  }

  elsif (($tmp) = ($arg =~ m/^-smbase=(.*)$/)) {
    $SMBASE = $tmp;
  }
  elsif (($tmp) = ($arg =~ m/^-ast=(.*)$/)) {
    $AST = $tmp;
  }
  elsif (($tmp) = ($arg =~ m/^-elkhound=(.*)$/)) {
    $ELKHOUND = $tmp;
  }
  elsif (($tmp) = ($arg =~ m/^-gnu=(.*)$/)) {
    $USE_GNU = $tmp;
  }
  elsif (($tmp) = ($arg =~ m/^-gnu$/)) {
    die "-gnu option must be followed by =yes or =no\n";
  }

  elsif ($arg eq "-useSerialNumbers") {
    push @CCFLAGS, "-DUSE_SERIAL_NUMBERS=1";
  }

  else {
    die "unknown option: $arg\n";
  }
}

if (!$debug) {
  if ($allow_dash_O2) {
    push @CCFLAGS, "-O2";
  }
  push @CCFLAGS, "-DNDEBUG";
}

if ($use_dash_g) {
  push @CCFLAGS, "-g";
  push @LDFLAGS, "-g";
}

$os = `uname -s`;
chomp($os);
if ($os eq "Linux") {
  push @CCFLAGS, "-D__LINUX__";
}

# if haven't seen a hashfunction by now, use the default
if (!grep (/^-D.*_HASH$/, @CCFLAGS)) {
  push @CCFLAGS, $default_hash;
}

# smash the list together to make a string
$CCFLAGS = join(' ', @CCFLAGS);
$LDFLAGS = join(' ', @LDFLAGS);


# ------------------ check for needed components ----------------
# smbase
if (! -f "$SMBASE/nonport.h") {
  die "I cannot find nonport.h in `$SMBASE'.\n" .
      "The smbase library is required for elsa.\n" .
      "If it's in a different location, use the -smbase=<dir> option.\n";
}

# ast
if (! -f "$AST/asthelp.h") {
  die "I cannot find asthelp.h in `$AST'.\n" .
      "The ast system is required for elsa.\n" .
      "If it's in a different location, use the -ast=<dir> option.\n";
}

# elkhound
if (! -f "$ELKHOUND/glr.h") {
  die "I cannot find glr.h in `$ELKHOUND'.\n" .
      "The elkhound system is required for elsa.\n" .
      "If it's in a different location, use the -elkhound=<dir> option.\n";
}

# use smbase's $BASE_FLAGS if I can find them
$smbase_flags = `$SMBASE/config.summary 2>/dev/null | grep BASE_FLAGS`;
if (defined($smbase_flags)) {
  ($BASE_FLAGS = $smbase_flags) =~ s|^.*: *||;
  chomp($BASE_FLAGS);
}


# ---------------------- etags? ---------------------
print("checking for etags... ");
if (system("type etags >/dev/null 2>&1")) {
  # doesn't have etags; cygwin is an example of such a system
  print("not found\n");
  $ETAGS = "true";       # 'true' is a no-op
}
elsif (system("etags --help | grep -- --members >/dev/null")) {
  # has it, but it does not know about the --members option
  print("etags\n");
  $ETAGS = "etags";
}
else {
  # assume if it knows about --members it knows about --typedefs too
  print("etags --members --typedefs\n");
  $ETAGS = "etags --members --typedefs";
}


# ------------------ config.summary -----------------
# create a program to summarize the configuration
open(OUT, ">config.summary") or die("can't make config.summary");
print OUT (<<"OUTER_EOF");
#!/bin/sh
# config.summary

cat <<EOF
./configure command:
  $0 $originalArgs

Elsa configuration summary:
  debug:       $debug

Compile flags:
  BASE_FLAGS:  $BASE_FLAGS
  CCFLAGS:     $CCFLAGS
  LDFLAGS:     $LDFLAGS
  SMBASE:      $SMBASE
  AST:         $AST
  ELKHOUND:    $ELKHOUND
  USE_GNU:     $USE_GNU

EOF

OUTER_EOF

close(OUT) or die;
chmod 0755, "config.summary";


# ------------------- config.status ------------------
# from here on, combine BASE_FLAGS and CCFLAGS
$CCFLAGS = "$BASE_FLAGS $CCFLAGS";

# create a program which will create the Makefile
open(OUT, ">config.status") or die("can't make config.status");
print OUT (<<"OUTER_EOF");
#!/bin/sh
# config.status

# this file was created by ./configure

# report on configuration
./config.summary


echo "creating Makefile ..."

# overcome my chmod below
rm -f Makefile

cat >Makefile <<EOF
# Makefile for elsa
# NOTE: do not edit; generated by:
#   $0 $originalArgs

EOF

# substitute variables
sed -e "s|\@CCFLAGS\@|$CCFLAGS|g" \\
    -e "s|\@LDFLAGS\@|$LDFLAGS|g" \\
    -e "s|\@SMBASE\@|$SMBASE|g" \\
    -e "s|\@AST\@|$AST|g" \\
    -e "s|\@ELKHOUND\@|$ELKHOUND|g" \\
    -e "s|\@USE_GNU\@|$USE_GNU|g" \\
    -e "s|\@ETAGS\@|$ETAGS|g" \\
  <Makefile.in >>Makefile || exit

# discourage editing
chmod a-w Makefile


OUTER_EOF

close(OUT) or die;
chmod 0755, "config.status";


# ----------------- final actions -----------------
# run the output file generator
my $code = system("./config.status");
if ($code != 0) {
  # hopefully ./config.status has already printed a message,
  # I'll just relay the status code
  if ($code >> 8) {                
    exit($code >> 8);
  }
  else {
    exit($code & 127);
  }
}


print("\nYou can now run make, usually called 'make' or 'gmake'.\n");

exit(0);
