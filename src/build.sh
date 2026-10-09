#!/bin/sh
# Test build with hooks: RC_EXTRA=-DRC_TEST_HOOKS RC_OUT=/tmp/rc_test.exe sh src/build.sh
# Cross-compiles TheRoyalCourt.exe on Linux (needs g++-mingw-w64-x86-64 and binutils-mingw-w64-x86-64)
set -e
cd "$(dirname "$0")"
x86_64-w64-mingw32-windres app.rc -O coff -o /tmp/rc_app_res.o
x86_64-w64-mingw32-g++ -std=c++17 -O2 -ffunction-sections -fdata-sections -fstack-protector-strong -Wall -Wextra -municode -mwindows -static -static-libgcc -static-libstdc++ -Wl,--gc-sections \
  -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -D_WIN32_IE=0x0A00 \
  $RC_EXTRA main.cpp /tmp/rc_app_res.o -o "${RC_OUT:-../TheRoyalCourt.exe}" -lcomctl32 -lcomdlg32 -ladvapi32 -lshell32 -lole32 -luuid -ldwmapi -luxtheme -lgdiplus -lwinhttp -lwinmm
x86_64-w64-mingw32-strip "${RC_OUT:-../TheRoyalCourt.exe}"
echo "Built ${RC_OUT:-../TheRoyalCourt.exe}"
