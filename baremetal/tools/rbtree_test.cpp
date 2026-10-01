// Host test for rbtree.cpp: its functions take the place of libstdc++'s
// (they're defined in this program, so they win over the shared library's).
// Random inserts and erases, checked against a sorted reference and the
// red-black invariants (_Rb_tree::__rb_verify).
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <random>
#include <stdio.h>

int main() {
    std::mt19937 rng(12345);
    int fails = 0;
    for (int round = 0; round < 200; round++) {
        std::_Rb_tree<int, int, std::_Identity<int>, std::less<int>> t;
        std::vector<int> ref;
        int range = 1 + (int)(rng() % 400);
        for (int op = 0; op < 2000; op++) {
            int v = (int)(rng() % range);
            if (rng() % 3) {
                if (t._M_insert_unique(v).second) ref.insert(std::lower_bound(ref.begin(), ref.end(), v), v);
            } else {
                size_t n = t.erase(v);
                auto it = std::lower_bound(ref.begin(), ref.end(), v);
                if ((it != ref.end() && *it == v) != (n == 1)) { fails++; break; }
                if (n) ref.erase(it);
            }
            if (op % 50 == 0 && !t.__rb_verify()) { printf("invariants broken, round %d op %d\n", round, op); fails++; break; }
        }
        if (!t.__rb_verify() || !std::equal(t.begin(), t.end(), ref.begin(), ref.end()) ||
            !std::equal(t.rbegin(), t.rend(), ref.rbegin(), ref.rend())) { printf("mismatch, round %d\n", round); fails++; }
    }
    // The containers Willy uses.
    std::map<std::string, std::vector<int>> m;
    for (int i = 0; i < 1000; i++) m[std::to_string(rng() % 300)].push_back(i);
    for (int i = 0; i < 500; i++) m.erase(std::to_string(rng() % 300));
    std::string prev;
    for (auto& kv : m) { if (!prev.empty() && !(prev < kv.first)) fails++; prev = kv.first; }
    std::set<int> s{5, 1, 9, 3};
    if (*s.begin() != 1 || *s.rbegin() != 9 || *--s.end() != 9) fails++;
    printf(fails ? "%d FAILED\n" : "all checks passed\n", fails);
    return fails != 0;
}
