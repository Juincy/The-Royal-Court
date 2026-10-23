#!/bin/sh
# usage: run_funcs.sh FROM TO  -- function-level differential test (C++ oracle vs C# oracle)
cd "$(dirname "$0")/.." || exit 1
bad=0
for s in $(seq $1 $2); do
  python3 tests/gen_funcs.py $s > /tmp/ff.txt
  /tmp/oracle_cpp2 funcs /tmp/ff.txt > /tmp/ff.cpp
  dotnet tests/RoyalCourt.Oracle/bin/Release/net10.0/RoyalCourt.Oracle.dll funcs /tmp/ff.txt > /tmp/ff.cs
  if ! cmp -s /tmp/ff.cpp /tmp/ff.cs; then echo "FUNCS DIFF seed $s"; diff /tmp/ff.cpp /tmp/ff.cs | head -6 | cut -c1-220; bad=$((bad+1)); fi
done
echo "funcs done $1..$2: $bad differing"
