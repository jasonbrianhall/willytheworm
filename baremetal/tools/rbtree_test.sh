#!/bin/sh
# Check rbtree.cpp against std::map's expectations, 64- and 32-bit.
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
for m in 64 32; do
    ${CXX:-g++} -m$m -std=gnu++17 -O2 -fsanitize=undefined rbtree_test.cpp ../rbtree.cpp -o "$T/t$m" 2>/dev/null || { echo "-m$m: no toolchain, skipped"; continue; }
    nm "$T/t$m" | grep -q " T _ZSt29_Rb_tree_insert_and_rebalance" || { echo "-m$m: not using rbtree.cpp"; exit 1; }
    printf -- "-m$m: "; "$T/t$m"
done
