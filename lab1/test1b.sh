#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'

echo 1234 > foo.txt

(cat foo.txt || echo foo is changed > foo.txt) && (catc foo.txt || cat foo.txt)

catc foo.txt && cat foo.txt

rm foo.txt

echo -e b\nD\ne\nF\nA\nb | tr a-z A-Z | sort -u || echo sort failed!
EOF


cat >test.exp <<'EOF'
1234
1234
A
B
D
E
F
EOF

../timetrash test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
}

) || exit

rm -fr "$tmp"
