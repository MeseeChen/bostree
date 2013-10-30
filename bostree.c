/*
	Self-Balancing order statistic tree

	Implements an AVL tree with two additional methods,
	Select(i), which finds the i'th smallest element, and
	Rank(x), which returns the rank of a given element,
	which both run in O(log n).

	Copyright (c) 2013, Phillip Berndt

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bostree.h"
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>

/* Macro to simplify the definition of left/right symmetric functions */
#define BOSTREE_LR_SYMMETRIC(x) \
	size_t _bosnode_left_offset        = x == 1 ? offsetof(BOSNode, left_child_node) : offsetof(BOSNode, right_child_node); \
	size_t _bosnode_right_offset       = x == 0 ? offsetof(BOSNode, left_child_node) : offsetof(BOSNode, right_child_node); \
	size_t _bosnode_left_count_offset  = x == 1 ? offsetof(BOSNode, left_child_count) : offsetof(BOSNode, right_child_count); \
	size_t _bosnode_right_count_offset = x == 0 ? offsetof(BOSNode, left_child_count) : offsetof(BOSNode, right_child_count)
#define BOSTREE_LR_LEFT_CHILD(x)  *(BOSNode**)((char*)x + _bosnode_left_offset)
#define BOSTREE_LR_RIGHT_CHILD(x) *(BOSNode**)((char*)x + _bosnode_right_offset)
#define BOSTREE_LR_RIGHT_CHILD_COUNT(x) *(unsigned int *)((char*)x + _bosnode_right_count_offset)
#define BOSTREE_LR_LEFT_CHILD_COUNT(x)  *(unsigned int *)((char*)x + _bosnode_left_count_offset)

/* Tree structure */
struct _BOSTree {
	BOSNode *root_node;

	BOSTree_cmp_function cmp_function;
};

/* Private helper methods */

/*
	Recalculate the depth of a node
*/
static void _bostree_depth_recalculate(BOSNode *node) {
	node->depth = node->left_child_node == NULL ? 0 : node->left_child_node->depth + 1;
	if(node->right_child_node == NULL) {
		return;
	}
	int depth = node->right_child_node->depth + 1;
	if(depth > node->depth) {
		node->depth = depth;
	}
}

/*
	Generic rotation method. See below in the _left & _right implementations
	for what it does.
*/
static int _bostree_rotate(BOSTree *tree, BOSNode *node, unsigned char side) {
	BOSTREE_LR_SYMMETRIC(side);

	if(node->parent_node == NULL) {
		return -1;
	}

	BOSNode *old_parent = node->parent_node;
	BOSNode *old_parent_parent = old_parent->parent_node;

	/* Make left child of node the new right child of parent node */
	if(BOSTREE_LR_LEFT_CHILD(node) != NULL) {
		(BOSTREE_LR_LEFT_CHILD(node))->parent_node = old_parent;
		assert((BOSTREE_LR_LEFT_CHILD(node))->parent_node != BOSTREE_LR_LEFT_CHILD(node));
	}
	BOSTREE_LR_RIGHT_CHILD(old_parent) = BOSTREE_LR_LEFT_CHILD(node);
	BOSTREE_LR_RIGHT_CHILD_COUNT(old_parent) = BOSTREE_LR_LEFT_CHILD_COUNT(node);
	_bostree_depth_recalculate(old_parent);

	/* Make old parent new left child of node */
	old_parent->parent_node = node;
	assert(old_parent->parent_node != old_parent);
	BOSTREE_LR_LEFT_CHILD(node) = old_parent;
	BOSTREE_LR_LEFT_CHILD_COUNT(node) = old_parent->right_child_count + old_parent->left_child_count + 1;
	_bostree_depth_recalculate(node);

	/* Replace parent node with new parent node */
	node->parent_node = old_parent_parent;
	assert(node->parent_node != node);
	if(old_parent_parent == NULL) {
		tree->root_node = node;
	}
	else if(BOSTREE_LR_LEFT_CHILD(old_parent_parent) == old_parent) {
		BOSTREE_LR_LEFT_CHILD(old_parent_parent) = node;
		_bostree_depth_recalculate(old_parent_parent);
	}
	else {
		BOSTREE_LR_RIGHT_CHILD(old_parent_parent) = node;
		_bostree_depth_recalculate(old_parent_parent);
	}

	return 0;
}

/*
  Left rotation. Transforms a tree as follows:

	 A            C
	   C    ->  A
	  F          F

  The parameter node must point to element C in the left picture. Returns 0 on
  success.
 */
static int _bostree_rotate_left(BOSTree *tree, BOSNode *node) {
	return _bostree_rotate(tree, node, 1);
}


/*
  Right rotation. Transforms the tree as follows:

	 A            B
   B        ->      A
	E              E

  The parameter node must point to B in the left picture. Returns 0 on
  success.
 */
static int _bostree_rotate_right(BOSTree *tree, BOSNode *node) {
	return _bostree_rotate(tree, node, 0);
}

/*
	Rebalances the tree at a given node. Returns the new node on the position
	where node was before, or node if nothing was changed.
*/
static BOSNode *_bostree_rebalance(BOSTree *tree, BOSNode *node) {
	int rdepth = node->right_child_node != NULL ? node->right_child_node->depth + 1 : 0;
	int ldepth = node->left_child_node != NULL ? node->left_child_node->depth + 1 : 0;
	int balance = ldepth - rdepth;

	if(balance > 1) {
		if(node->left_child_node->right_child_node != NULL && (node->left_child_node->left_child_node == NULL || node->left_child_node->right_child_node->depth > node->left_child_node->left_child_node->depth)) {
			_bostree_rotate_left(tree, node->left_child_node->right_child_node);
		}

		_bostree_rotate_right(tree, node->left_child_node);
		node = node->parent_node;
	}
	else if(balance < -1) {
		if(node->right_child_node->left_child_node != NULL && (node->right_child_node->right_child_node == NULL || node->right_child_node->left_child_node->depth > node->right_child_node->right_child_node->depth)) {
			_bostree_rotate_right(tree, node->right_child_node->left_child_node);
		}

		_bostree_rotate_left(tree, node->right_child_node);
		node = node->parent_node;
	}

	return node;
}

/* Public methods */

/*
	Create a new tree structure
*/
BOSTree *bostree_new(BOSTree_cmp_function cmp_function) {
	BOSTree *new_tree = (BOSTree *)calloc(1, sizeof(BOSTree));
	new_tree->cmp_function = cmp_function;

	return new_tree;
}

/*
	Free the memory used by a tree structure and all of its nodes.
*/
void bostree_destroy(BOSTree *tree) {
	while(tree->root_node != NULL) {
		bostree_remove(tree, bostree_select(tree, 0));
	}
	free(tree);
}

/*
	Return the number of nodes in a tree.
*/
unsigned int bostree_node_count(BOSTree *tree) {
	if(tree->root_node == NULL) {
		return 0;
	}

	return 1 + tree->root_node->left_child_count + tree->root_node->right_child_count;
}

/*
	Insert data into the tree. The key is used for indexing and fed into the
	compare-function. data is a pointer to arbitrary data.

	Returns the newly created node.
*/
BOSNode *bostree_insert(BOSTree *tree, void *key, void *data) {
	BOSNode *new_node = (BOSNode *)calloc(sizeof(BOSNode), 1);
	new_node->weak_ref_node_valid = 1;
	new_node->weak_ref_count = 1;
	new_node->key = key;
	new_node->data = data;

	/*
		Search for the correct insert position and increment the child count
		along the path
	*/
	BOSNode **search = &tree->root_node;
	while(*search != NULL) {
		new_node->parent_node = *search;
		int direction = tree->cmp_function(key, (*search)->key);
		if(direction <= 0) {
			(*search)->left_child_count++;
			search = &((*search)->left_child_node);
		}
		else {
			(*search)->right_child_count++;
			search = &((*search)->right_child_node);
		}
	}
	*search = new_node;

	/*
		Fix depth and rebalance upwards to the root
	*/
	BOSNode *bubble = new_node;
	while(bubble != NULL) {
		if(bubble->parent_node != NULL) {
			if(bubble->parent_node->depth < bubble->depth + 1) {
				bubble->parent_node->depth++;
			}
			else {
				bubble = _bostree_rebalance(tree, bubble);
				break;
			}
		}

		bubble = _bostree_rebalance(tree, bubble);
		bubble = bubble->parent_node;
	}

	return new_node;
}

/*
	Remove a given node from the tree. The argument must be a node, not the
	associated key! You can look up nodes using bostree_lookup(). The memory
	for the key/value is not freed, the node structure itself is.

	So you must call
		free(node->key);
		free(node->value);
		bostree_remove(tree, node);
	to cleanly remove an entry.
*/
void bostree_remove(BOSTree *tree, BOSNode *node) {
	if(node == NULL) {
		return;
	}

	BOSNode **reparent_location;
	if(node == tree->root_node) {
		reparent_location = &tree->root_node;
	}
	else if(node->parent_node->left_child_node == node) {
		reparent_location = &node->parent_node->left_child_node;
	}
	else {
		reparent_location = &node->parent_node->right_child_node;
	}

	/* If this node had only one child, simply replace by that one */
	if(node->left_child_node == NULL) {
		*reparent_location = node->right_child_node;
	}
	else if(node->right_child_node == NULL) {
		*reparent_location = node->left_child_node;
	}
	/* If not, we find the largest element of the left sub-tree and use that */
	else {
		BOSNode *replacer = bostree_previous_node(node);
		BOSNode *iterator = replacer;
		if(iterator->parent_node->left_child_node == iterator) {
			iterator->parent_node->left_child_node = NULL;
			iterator->parent_node->left_child_count = 0;
			iterator->parent_node->depth = iterator->parent_node->right_child_node != NULL ? iterator->parent_node->right_child_node->depth + 1 : 0;
		}
		else {
			iterator->parent_node->right_child_node = NULL;
			iterator->parent_node->right_child_count = 0;
			iterator->parent_node->depth = iterator->parent_node->left_child_node != NULL ? iterator->parent_node->left_child_node->depth + 1 : 0;
		}
		iterator = iterator->parent_node;
		while(iterator != *reparent_location && iterator->parent_node != NULL) {
			if(iterator->parent_node->left_child_node == iterator) {
				iterator->parent_node->left_child_count--;
				_bostree_depth_recalculate(iterator->parent_node);
			}
			else {
				iterator->parent_node->right_child_count--;
				_bostree_depth_recalculate(iterator->parent_node);
			}
			iterator = iterator->parent_node;
		}

		/* Reposition replacer */
		*reparent_location = replacer;
		replacer->right_child_node = node->right_child_node;
		replacer->right_child_count = node->right_child_count;
		if(replacer->right_child_node != NULL) {
			replacer->right_child_node->parent_node = replacer;
			assert(replacer->right_child_node->parent_node != replacer->right_child_node);
		}
		replacer->left_child_node = node->left_child_node;
		replacer->left_child_count = node->left_child_count;
		if(replacer->left_child_node != NULL) {
			replacer->left_child_node->parent_node = replacer;
			assert(replacer->left_child_node->parent_node != replacer->left_child_node);
		}
		_bostree_depth_recalculate(replacer);
	}

	if(*reparent_location != NULL) {
		(*reparent_location)->parent_node = node->parent_node;
		assert((*reparent_location)->parent_node != *reparent_location);
		*reparent_location = _bostree_rebalance(tree, *reparent_location);
	}

	/* Fix depth and child count at the parent node */
	if(node->parent_node != NULL) {
		/*
			Comparing the memory addresses instead of the contents here is
			important if *reparent_location == NULL
		*/
		if(&node->parent_node->left_child_node == reparent_location) {
			node->parent_node->left_child_count--;
		}
		else {
			node->parent_node->right_child_count--;
		}

		_bostree_depth_recalculate(node->parent_node);

		/* Fix depth and child counts down to the root */
		BOSNode *iterator = node->parent_node;
		while(iterator->parent_node != NULL) {
			if(iterator->parent_node->left_child_node == iterator) {
				iterator->parent_node->left_child_count--;
			}
			else {
				iterator->parent_node->right_child_count--;
			}

			iterator = _bostree_rebalance(tree, iterator);
			_bostree_depth_recalculate(iterator->parent_node);
			iterator = iterator->parent_node;
		}
	}
	if(tree->root_node != NULL) {
		_bostree_rebalance(tree, tree->root_node);
	}

	node->weak_ref_node_valid = 0;
	bostree_node_weak_unref(node);
}

/*
	Create a weak reference to a node

	This simple implementation of weak references really only deletes the node
	once the last reference to it has been dropped, but it invalidates the node
	once it has been removed from the tree.
*/
BOSNode *bostree_node_weak_ref(BOSNode *node) {
	node->weak_ref_count++;
	return node;
}

/*
	Remove the weak reference again, returning NULL if the node was unvalid and
	the node if it is still valid
*/
BOSNode *bostree_node_weak_unref(BOSNode *node) {
	BOSNode *retval = node->weak_ref_node_valid ? node : NULL;
	if(--node->weak_ref_count == 0) {
		free(node);
	}
	return retval;
}

/*
	Lookup the node for a given key.

	Returns NULL if the node is not found.
*/
BOSNode *bostree_lookup(BOSTree *tree, void *key) {
	BOSNode *node = tree->root_node;
	while(node != NULL) {
		int direction = tree->cmp_function(key, node->key);
		if(direction < 0) {
			node = node->left_child_node;
		}
		else if(direction == 0) {
			break;
		}
		else {
			node = node->right_child_node;
		}
	}
	return node;
}

/*
	Returns the node for a given index.

	Returns NULL if a node with the given index does not exist.
*/
BOSNode *bostree_select(BOSTree *tree, unsigned int index) {
	BOSNode *node = tree->root_node;
	while(node != NULL) {
		if(node->left_child_count <= index) {
			index -= node->left_child_count;
			if(index == 0) {
				return node;
			}
			index--;
			node = node->right_child_node;
		}
		else {
			node = node->left_child_node;
		}
	}
	return NULL;
}

/*
	Return the node following node in in-order-traversal.

	Returns NULL when called with the last node.
*/
BOSNode *bostree_next_node(BOSNode *node) {
	if(node->right_child_node != NULL) {
		node = node->right_child_node;
		while(node->left_child_node != NULL) {
			node = node->left_child_node;
		}
		return node;
	}
	while(node->parent_node != NULL) {
		if(node->parent_node->left_child_node == node) {
			return node->parent_node;
		}
		node = node->parent_node;
	}
	return NULL;
}

/*
	Return the node preceeding node in in-order-traversal.

	Returns NULL when called with the first node.
*/
BOSNode *bostree_previous_node(BOSNode *node) {
	if(node->left_child_node != NULL) {
		node = node->left_child_node;
		while(node->right_child_node != NULL) {
			node = node->right_child_node;
		}
		return node;
	}
	while(node->parent_node != NULL) {
		if(node->parent_node->right_child_node == node) {
			return node->parent_node;
		}
		node = node->parent_node;
	}
	return NULL;
}

/*
	Returns the rank associated with node.
*/
unsigned int bostree_rank(BOSNode *node) {
	unsigned int rank = node->left_child_count;
	while(node->parent_node != NULL) {
		if(node->parent_node->right_child_node == node) {
			rank += node->parent_node->left_child_count + 1;
		}
		node = node->parent_node;
	}
	return rank;
}

#ifndef NDEBUG
#include <stdio.h>
#include <unistd.h>

/* Debug helpers:

	Print the tree to stdout. Makes use of console codes, so this will only
	look neat on vt100 compatible terminals, i.e. unix/linux consoles.
*/

void _bostree_print_helper(BOSNode *node, unsigned int indent, unsigned int level) {
	printf("\033[%d;%dH", level + 1, indent);
	fsync(0);
	printf("%s(%d,%d,%d)", (char *)node->key, node->left_child_count, node->right_child_count, node->depth);

	if(node->left_child_node != NULL) {
		_bostree_print_helper(node->left_child_node, indent - (2 << node->depth), level + 1);
	}
	if(node->right_child_node != NULL) {
		_bostree_print_helper(node->right_child_node, indent + (2 << node->depth), level + 1);
	}
}

void bostree_print(BOSTree *tree) {
	if(tree->root_node == NULL) {
		return;
	}

	puts("\033[2J");
	fsync(0);

	_bostree_print_helper(tree->root_node, (4 << tree->root_node->depth), 0);

	printf("\033[%d;1H", tree->root_node->depth + 2);
	fsync(0);
}
#endif
