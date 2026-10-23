#!/bin/sh
# usage: run_diff.sh FROM TO   - differential test of the C++ core against the C# core on random scenarios
FROM=${1:-1}; TO=${2:-50}
CPP=/tmp/oracle_cpp
CS=/home/claude/rc-cs/tests/RoyalCourt.Oracle/bin/Release/net10.0/RoyalCourt.Oracle.dll
mkdir -p /tmp/difft
bad=0
for s in $(seq $FROM $TO); do
  d=/tmp/difft/s$s
  python3 /home/claude/rc-cs/tests/gen_scen.py $s $d || { echo "gen fail $s"; bad=$((bad+1)); continue; }
  $CPP scenario $d > $d.cpp.txt 2>$d.cpp.err
  dotnet $CS scenario $d > $d.cs.txt 2>$d.cs.err
  if ! cmp -s $d.cpp.txt $d.cs.txt; then
    echo "DIFF seed $s"; diff $d.cpp.txt $d.cs.txt | cut -c1-220 | head -6; bad=$((bad+1))
  else rm -rf $d $d.cpp.txt $d.cs.txt $d.cpp.err $d.cs.err; fi
done
echo "done $FROM..$TO: $bad differing"
