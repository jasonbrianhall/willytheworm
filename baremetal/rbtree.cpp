// std::map / std::set's red-black tree balancing, normally libstdc++'s
// tree.cc. Linking that object from libstdc++.a pulls in code built for the
// distribution's CPU baseline (the 32-bit one uses cmov, a Pentium Pro
// instruction), so the kernel carries its own: the same node layout the
// headers use (<bits/stl_tree.h>), and the textbook algorithms.
// Host test: tools/rbtree_test.sh.
#include <bits/stl_tree.h>

namespace std _GLIBCXX_VISIBILITY(default) {
_GLIBCXX_BEGIN_NAMESPACE_VERSION

typedef _Rb_tree_node_base Node;

static Node* local_increment(Node* x) throw() {
    if (x->_M_right) {
        x = x->_M_right;
        while (x->_M_left) x = x->_M_left;
        return x;
    }
    Node* y = x->_M_parent;
    while (x == y->_M_right) { x = y; y = y->_M_parent; }
    // At the header, x is the root and y its parent (the header): stay at the header.
    return x->_M_right != y ? y : x;
}
static Node* local_decrement(Node* x) throw() {
    if (x->_M_color == _S_red && x->_M_parent->_M_parent == x)   // the header: go to the rightmost node
        return x->_M_right;
    if (x->_M_left) {
        Node* y = x->_M_left;
        while (y->_M_right) y = y->_M_right;
        return y;
    }
    Node* y = x->_M_parent;
    while (x == y->_M_left) { x = y; y = y->_M_parent; }
    return y;
}

Node* _Rb_tree_increment(Node* x) throw() { return local_increment(x); }
const Node* _Rb_tree_increment(const Node* x) throw() { return local_increment(const_cast<Node*>(x)); }
Node* _Rb_tree_decrement(Node* x) throw() { return local_decrement(x); }
const Node* _Rb_tree_decrement(const Node* x) throw() { return local_decrement(const_cast<Node*>(x)); }

static void rotate_left(Node* x, Node*& root) {
    Node* y = x->_M_right;
    x->_M_right = y->_M_left;
    if (y->_M_left) y->_M_left->_M_parent = x;
    y->_M_parent = x->_M_parent;
    if (x == root) root = y;
    else if (x == x->_M_parent->_M_left) x->_M_parent->_M_left = y;
    else x->_M_parent->_M_right = y;
    y->_M_left = x;
    x->_M_parent = y;
}
static void rotate_right(Node* x, Node*& root) {
    Node* y = x->_M_left;
    x->_M_left = y->_M_right;
    if (y->_M_right) y->_M_right->_M_parent = x;
    y->_M_parent = x->_M_parent;
    if (x == root) root = y;
    else if (x == x->_M_parent->_M_right) x->_M_parent->_M_right = y;
    else x->_M_parent->_M_left = y;
    y->_M_right = x;
    x->_M_parent = y;
}

// The header node: parent = root, left = leftmost, right = rightmost.
void _Rb_tree_insert_and_rebalance(const bool insert_left, Node* x, Node* p, Node& header) throw() {
    Node*& root = header._M_parent;
    x->_M_parent = p;
    x->_M_left = x->_M_right = nullptr;
    x->_M_color = _S_red;
    if (insert_left) {
        p->_M_left = x;                       // also makes leftmost = x when p == &header
        if (p == &header) { header._M_parent = x; header._M_right = x; }
        else if (p == header._M_left) header._M_left = x;
    } else {
        p->_M_right = x;
        if (p == header._M_right) header._M_right = x;
    }
    while (x != root && x->_M_parent->_M_color == _S_red) {
        Node* xpp = x->_M_parent->_M_parent;
        if (x->_M_parent == xpp->_M_left) {
            Node* y = xpp->_M_right;
            if (y && y->_M_color == _S_red) {
                x->_M_parent->_M_color = _S_black;
                y->_M_color = _S_black;
                xpp->_M_color = _S_red;
                x = xpp;
            } else {
                if (x == x->_M_parent->_M_right) { x = x->_M_parent; rotate_left(x, root); }
                x->_M_parent->_M_color = _S_black;
                xpp->_M_color = _S_red;
                rotate_right(xpp, root);
            }
        } else {
            Node* y = xpp->_M_left;
            if (y && y->_M_color == _S_red) {
                x->_M_parent->_M_color = _S_black;
                y->_M_color = _S_black;
                xpp->_M_color = _S_red;
                x = xpp;
            } else {
                if (x == x->_M_parent->_M_left) { x = x->_M_parent; rotate_right(x, root); }
                x->_M_parent->_M_color = _S_black;
                xpp->_M_color = _S_red;
                rotate_left(xpp, root);
            }
        }
    }
    root->_M_color = _S_black;
}

Node* _Rb_tree_rebalance_for_erase(Node* const z, Node& header) throw() {
    Node*& root = header._M_parent;
    Node*& leftmost = header._M_left;
    Node*& rightmost = header._M_right;
    Node* y = z;
    Node* x = nullptr;
    Node* x_parent = nullptr;

    if (!y->_M_left) x = y->_M_right;                 // z has at most one child: y = z
    else if (!y->_M_right) x = y->_M_left;
    else {                                            // two children: y = z's successor
        y = y->_M_right;
        while (y->_M_left) y = y->_M_left;
        x = y->_M_right;
    }
    if (y != z) {                                     // relink y in place of z
        z->_M_left->_M_parent = y;
        y->_M_left = z->_M_left;
        if (y != z->_M_right) {
            x_parent = y->_M_parent;
            if (x) x->_M_parent = y->_M_parent;
            y->_M_parent->_M_left = x;
            y->_M_right = z->_M_right;
            z->_M_right->_M_parent = y;
        } else {
            x_parent = y;
        }
        if (root == z) root = y;
        else if (z->_M_parent->_M_left == z) z->_M_parent->_M_left = y;
        else z->_M_parent->_M_right = y;
        y->_M_parent = z->_M_parent;
        _Rb_tree_color c = y->_M_color; y->_M_color = z->_M_color; z->_M_color = c;
        y = z;                                        // y now points to the node actually removed
    } else {
        x_parent = y->_M_parent;
        if (x) x->_M_parent = y->_M_parent;
        if (root == z) root = x;
        else if (z->_M_parent->_M_left == z) z->_M_parent->_M_left = x;
        else z->_M_parent->_M_right = x;
        if (leftmost == z) {
            if (!z->_M_right) leftmost = z->_M_parent;   // z->_M_left must be null too
            else leftmost = _Rb_tree_node_base::_S_minimum(x);
        }
        if (rightmost == z) {
            if (!z->_M_left) rightmost = z->_M_parent;
            else rightmost = _Rb_tree_node_base::_S_maximum(x);
        }
    }
    if (y->_M_color != _S_red) {                      // removed a black node: fix up
        while (x != root && (!x || x->_M_color == _S_black)) {
            if (x == x_parent->_M_left) {
                Node* w = x_parent->_M_right;
                if (w->_M_color == _S_red) {
                    w->_M_color = _S_black;
                    x_parent->_M_color = _S_red;
                    rotate_left(x_parent, root);
                    w = x_parent->_M_right;
                }
                if ((!w->_M_left || w->_M_left->_M_color == _S_black) &&
                    (!w->_M_right || w->_M_right->_M_color == _S_black)) {
                    w->_M_color = _S_red;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (!w->_M_right || w->_M_right->_M_color == _S_black) {
                        w->_M_left->_M_color = _S_black;
                        w->_M_color = _S_red;
                        rotate_right(w, root);
                        w = x_parent->_M_right;
                    }
                    w->_M_color = x_parent->_M_color;
                    x_parent->_M_color = _S_black;
                    if (w->_M_right) w->_M_right->_M_color = _S_black;
                    rotate_left(x_parent, root);
                    break;
                }
            } else {
                Node* w = x_parent->_M_left;
                if (w->_M_color == _S_red) {
                    w->_M_color = _S_black;
                    x_parent->_M_color = _S_red;
                    rotate_right(x_parent, root);
                    w = x_parent->_M_left;
                }
                if ((!w->_M_right || w->_M_right->_M_color == _S_black) &&
                    (!w->_M_left || w->_M_left->_M_color == _S_black)) {
                    w->_M_color = _S_red;
                    x = x_parent;
                    x_parent = x_parent->_M_parent;
                } else {
                    if (!w->_M_left || w->_M_left->_M_color == _S_black) {
                        w->_M_right->_M_color = _S_black;
                        w->_M_color = _S_red;
                        rotate_left(w, root);
                        w = x_parent->_M_left;
                    }
                    w->_M_color = x_parent->_M_color;
                    x_parent->_M_color = _S_black;
                    if (w->_M_left) w->_M_left->_M_color = _S_black;
                    rotate_right(x_parent, root);
                    break;
                }
            }
        }
        if (x) x->_M_color = _S_black;
    }
    return y;
}

unsigned int _Rb_tree_black_count(const Node* node, const Node* root) throw() {
    if (!node) return 0;
    unsigned int sum = 0;
    do {
        if (node->_M_color == _S_black) ++sum;
        if (node == root) break;
        node = node->_M_parent;
    } while (true);
    return sum;
}

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace std
