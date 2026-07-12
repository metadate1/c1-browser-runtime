#include <assert.h>

#include "util/tree.h"

static int value_a;
static int value_b;
static int value_c;
static int value_d;
static int value_n;
static int value_x;
static int value_y;

static tree_node_t *new_node(void *data) {
  tree_node_t *node = tree_node_new();
  node->data = data;
  return node;
}

static void assert_links(tree_node_t *node, tree_node_t *parent) {
  tree_node_t *child;
  tree_node_t *prev = 0;

  assert(node->parent == parent);
  for (child=node->child; child; child=child->next) {
    assert(child->prev == prev);
    assert_links(child, node);
    prev = child;
  }
  assert(node->tail_child == prev);
}

static void assert_same_tree(tree_node_t *actual, tree_node_t *expected) {
  tree_node_t *a;
  tree_node_t *e;

  assert(actual != 0);
  assert(expected != 0);
  assert(actual->data == expected->data);
  a = actual->child;
  e = expected->child;
  while (a && e) {
    assert_same_tree(a, e);
    a = a->next;
    e = e->next;
  }
  assert(a == 0 && e == 0);
}

static void test_root_promotion_owns_inherited_children(void) {
  tree_node_t *old_root = new_node(&value_a);
  tree_node_t *promoted = new_node(&value_b);
  tree_node_t *existing_child = new_node(&value_d);
  tree_node_t *inherited_child = new_node(&value_c);

  tree_add_node(promoted, existing_child);
  tree_add_node(old_root, promoted);
  tree_add_node(old_root, inherited_child);

  assert(tree_remove_node_inherit(old_root) == promoted);
  assert(promoted->parent == 0);
  assert(promoted->prev == 0 && promoted->next == 0);
  assert(promoted->child == existing_child);
  assert(existing_child->next == inherited_child);
  assert(inherited_child->prev == existing_child);
  assert(promoted->tail_child == inherited_child);
  assert(old_root->child == 0 && old_root->tail_child == 0);
  assert_links(promoted, 0);

  tree_node_free(old_root, 0);
  tree_node_free(promoted, 1);
}

static void test_nonroot_removal_detaches_inherited_children(void) {
  tree_node_t *root = new_node(&value_a);
  tree_node_t *left = new_node(&value_x);
  tree_node_t *removed = new_node(&value_n);
  tree_node_t *right = new_node(&value_y);
  tree_node_t *first = new_node(&value_b);
  tree_node_t *second = new_node(&value_c);

  tree_add_node(root, left);
  tree_add_node(root, removed);
  tree_add_node(root, right);
  tree_add_node(removed, first);
  tree_add_node(removed, second);

  assert(tree_remove_node_inherit(removed) == root);
  assert(left->next == right);
  assert(right->next == first);
  assert(first->next == second);
  assert(root->tail_child == second);
  assert(removed->parent == 0);
  assert(removed->child == 0 && removed->tail_child == 0);
  assert_links(root, 0);

  tree_node_free(removed, 0);
  tree_node_free(root, 1);
}

static void test_leaf_root_removal_returns_empty_tree(void) {
  tree_node_t *leaf = new_node(&value_a);

  assert(tree_remove_node_inherit(leaf) == 0);
  assert(leaf->parent == 0);
  assert(leaf->child == 0 && leaf->tail_child == 0);
  tree_node_free(leaf, 0);
}

static void test_changes_apply_root_removal(void) {
  tree_node_t *src = new_node(&value_a);
  tree_node_t *dst = new_node(&value_b);
  tree_node_t *applied;
  tree_delta_t *delta;
  list_t *deltas;

  tree_add_node(src, new_node(&value_b));
  tree_add_node(src, new_node(&value_c));
  tree_add_node(dst, new_node(&value_c));

  deltas = tree_changes(src, dst);
  assert(list_length(deltas) == 1);
  delta = deltas->head->data;
  assert(delta->op == 2);
  assert(delta->value == &value_a);

  applied = tree_apply(src, deltas);
  assert_same_tree(applied, dst);
  assert_links(applied, 0);

  tree_node_free(applied, 1);
  tree_node_free(dst, 1);
  tree_node_free(src, 1);
  list_free(deltas, 3);
}

static void test_apply_can_remove_last_root(void) {
  tree_node_t *src = new_node(&value_a);
  tree_delta_t *delta;
  list_t *deltas = tree_changes(src, 0);

  assert(list_length(deltas) == 1);
  delta = deltas->head->data;
  assert(delta->op == 2);
  assert(delta->value == &value_a);

  assert(tree_apply(src, deltas) == 0);

  tree_node_free(src, 1);
  list_free(deltas, 3);
}

static void test_changes_apply_disjoint_roots(void) {
  tree_node_t *src = new_node(&value_a);
  tree_node_t *dst = new_node(&value_b);
  tree_node_t *applied;
  tree_delta_t *delta;
  list_t *deltas;
  int saw_new_root = 0;

  tree_add_node(src, new_node(&value_x));
  tree_add_node(dst, new_node(&value_c));
  tree_add_node(dst->child, new_node(&value_d));

  deltas = tree_changes(src, dst);
  list_for_each(deltas, delta) {
    if (delta->op == 5 && delta->value == &value_b) saw_new_root = 1;
  }
  assert(saw_new_root);

  applied = tree_apply(src, deltas);
  assert_same_tree(applied, dst);
  assert_links(applied, 0);

  tree_node_free(applied, 1);
  tree_node_free(dst, 1);
  tree_node_free(src, 1);
  list_free(deltas, 3);
}

static void test_changes_apply_from_empty_source(void) {
  tree_node_t *dst = new_node(&value_b);
  tree_node_t *applied;
  tree_delta_t *delta;
  list_t *deltas;

  tree_add_node(dst, new_node(&value_c));
  tree_add_node(dst, new_node(&value_d));

  deltas = tree_changes(0, dst);
  assert(list_length(deltas) == 3);
  delta = deltas->head->data;
  assert(delta->op == 5);
  assert(delta->value == &value_b);

  applied = tree_apply(0, deltas);
  assert_same_tree(applied, dst);
  assert_links(applied, 0);

  tree_node_free(applied, 1);
  tree_node_free(dst, 1);
  list_free(deltas, 3);
}

static void test_changes_apply_existing_descendant_as_root(void) {
  tree_node_t *src = new_node(&value_a);
  tree_node_t *dst = new_node(&value_b);
  tree_node_t *applied;
  tree_delta_t *delta;
  list_t *deltas;
  int saw_root_recreate = 0;

  tree_add_node(src, new_node(&value_b));
  tree_add_node(dst, new_node(&value_a));

  deltas = tree_changes(src, dst);
  list_for_each(deltas, delta) {
    if (delta->op == 5 && delta->value == &value_b) saw_root_recreate = 1;
  }
  assert(saw_root_recreate);

  applied = tree_apply(src, deltas);
  assert_same_tree(applied, dst);
  assert_links(applied, 0);

  tree_node_free(applied, 1);
  tree_node_free(dst, 1);
  tree_node_free(src, 1);
  list_free(deltas, 3);
}

static void test_changes_apply_descendant_root_with_siblings(void) {
  tree_node_t *src = new_node(&value_a);
  tree_node_t *src_b = new_node(&value_b);
  tree_node_t *dst = new_node(&value_b);
  tree_node_t *dst_a = new_node(&value_a);
  tree_node_t *applied;
  list_t *deltas;

  tree_add_node(src, new_node(&value_x));
  tree_add_node(src, src_b);
  tree_add_node(src_b, new_node(&value_y));

  tree_add_node(dst, dst_a);
  tree_add_node(dst_a, new_node(&value_x));
  tree_add_node(dst, new_node(&value_y));

  deltas = tree_changes(src, dst);
  applied = tree_apply(src, deltas);
  assert_same_tree(applied, dst);
  assert_links(applied, 0);

  tree_node_free(applied, 1);
  tree_node_free(dst, 1);
  tree_node_free(src, 1);
  list_free(deltas, 3);
}

int main(void) {
  test_root_promotion_owns_inherited_children();
  test_nonroot_removal_detaches_inherited_children();
  test_leaf_root_removal_returns_empty_tree();
  test_changes_apply_root_removal();
  test_apply_can_remove_last_root();
  test_changes_apply_disjoint_roots();
  test_changes_apply_from_empty_source();
  test_changes_apply_existing_descendant_as_root();
  test_changes_apply_descendant_root_with_siblings();
  return 0;
}
