#!/usr/bin/perl
# Print the initialized data references of an OS-9 program module (the
# table at header M$IRefs): the references to code, then to data, each
# list sorted, as l68 and elf2mod order them differently.
use strict; use warnings;
local $/; open my $f, '<:raw', $ARGV[0] or die "$ARGV[0]: $!\n"; my $m = <$f>;
my $p = unpack 'N', substr($m, 0x44, 4);
for my $list ('code', 'data') {
  my @refs;
  while (1) {
    my ($hi, $n) = unpack 'n n', substr($m, $p, 4);
    $p += 4;
    last if $n == 0;
    for (1 .. $n) {
      push @refs, ($hi << 16) | unpack('n', substr($m, $p, 2));
      $p += 2;
    }
  }
  printf "%s: %s\n", $list, join(' ', map { sprintf '%x', $_ } sort { $a <=> $b } @refs);
}
