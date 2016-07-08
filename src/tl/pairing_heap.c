#include "pairing_heap.h"

tl_ph_node tl_ph_null = {&tl_ph_null, &tl_ph_null.left_child, &tl_ph_null};

tl_ph_node* tl_ph_combine_siblings_(tl_ph_node* el, tl_ph_node* (*complink)(tl_ph_node* a, tl_ph_node* b)) {
	tl_ph_node *first = el, *second;
	tl_ph_node *stack = &tl_ph_null;

	while (first != &tl_ph_null) {
		tl_ph_node *tree, *next;
		second = first->right_sibling;
		if (second == &tl_ph_null) {
			first->right_sibling = stack;
			stack = first;
			break;
		}

		next = second->right_sibling;
		tree = complink(first, second);
		first = next;

		tree->right_sibling = stack;
		stack = tree;
	}

	second = stack->right_sibling;

	while (second != &tl_ph_null) {
		stack = complink(stack, second);
		second = second->right_sibling;
	}

	return stack;
}
