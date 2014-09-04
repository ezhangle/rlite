#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "btree.h"

int long_cmp(void* v1, void* v2) {
   long a = *((long*)v1), b = *((long*)v2);
   if (a == b) {
      return 0;
   }
   return a > b ? 1 : -1;
}

void long_formatter(void* v2, char** formatted, int* size) {
   *formatted = malloc(sizeof(char) * 22);
   *size = snprintf(*formatted, 22, "%ld", *(long*)v2);
}

void init_long_set() {
   long_set.score_size = sizeof(long);
   long_set.value_size = 0;
   long_set.cmp = long_cmp;
   long_set.formatter = long_formatter;
}

void init_long_hash() {
   long_set.score_size = sizeof(long);
   long_set.value_size = sizeof(void*);
   long_set.cmp = long_cmp;
   long_set.formatter = long_formatter;
}

rl_tree_node* rl_tree_node_create(rl_tree* tree) {
   rl_tree_node* node = malloc(sizeof(rl_tree_node));
   node->scores = malloc(sizeof(void*) * tree->max_size);
   node->children = NULL;
   if (tree->type->value_size) {
      node->values = malloc(sizeof(void*) * tree->max_size);
   } else {
      node->values = NULL;
   }
   node->size = 0;
   return node;
}

rl_tree* rl_tree_create(rl_tree_type* type, long max_size, rl_accessor* accessor) {
   rl_tree* tree = malloc(sizeof(rl_tree));
   if (!tree) {
      return NULL;
   }
   tree->max_size = max_size;
   tree->type = type;
   tree->root = rl_tree_node_create(tree);
   if (!tree->root) {
      free(tree);
      return NULL;
   }
   tree->root->size = 0;
   tree->accessor = accessor;
   tree->height = 1;
   return tree;
}

long rl_tree_find_score(rl_tree* tree, void* score, rl_tree_node*** nodes, long** positions) {
   rl_tree_node* node = tree->root;
   if ((!nodes && positions) || (nodes && !positions)) {
      return -1;
   }
   long i, pos, min, max;
   int cmp = 0;
   for (i = 0; i < tree->height; i++) {
      if (nodes) {
         (*nodes)[i] = node;
      }
      pos = 0;
      min = 0;
      max = node->size - 1;
      while (min <= max) {
         pos = (max - min) / 2;
         cmp = tree->type->cmp(score, node->scores[pos]);
         if (cmp == 0) {
            if (nodes) {
               for (;i < tree->height; i++) {
                  (*nodes)[i] = NULL;
               }
            }
            return 1;
         } else if (cmp > 0) {
            if (min == pos) {
               pos++;
               break;
            }
            min = pos;
         } else {
            if (max == pos) { break; }
            max = pos;
         }
      }
      if (positions) {
         if (pos == max && tree->type->cmp(score, node->scores[pos]) == 1) {
            pos++;
         }
         (*positions)[i] = pos;
      }
      if (node->children) {
         node = tree->accessor->getter(tree, node->children[pos]);
      }
   }
   return 0;
}

int rl_tree_add_child(rl_tree* tree, void* score, void* value) {
   rl_tree_node** nodes = malloc(sizeof(rl_tree_node*) * tree->height);
   long* positions = malloc(sizeof(long) * tree->height);
   long found = rl_tree_find_score(tree, score, &nodes, &positions);
   long i, pos;
   long child = -1;
   int retval = 0;

   if (found) {
      retval = 1;
      goto cleanup;
   }
   rl_tree_node* node;
   for (i = tree->height - 1; i >= 0; i--) {
      node = nodes[i];
      if (node->size < tree->max_size) {
         memmove(&node->scores[positions[i] + 1], &node->scores[positions[i]], sizeof(void*) * (node->size - positions[i]));
         if (node->values) {
            memmove(&node->values[positions[i] + 1], &node->values[positions[i]], sizeof(void*) * (node->size - positions[i]));
         }
         if (node->children) {
            memmove(&node->children[positions[i] + 2], &node->children[positions[i] + 1], sizeof(long) * (node->size - positions[i]));
         }
         node->scores[positions[i]] = score;
         if (node->values) {
            node->values[positions[i]] = value;
         }
         if (child != -1) {
            if (!node->children) {
               fprintf(stderr, "Adding child, but children is not initialized\n");
            }
            node->children[positions[i] + 1] = child;
         }
         node->size++;
         score = NULL;
         break;
      } else {
         // TODO: copy values
         pos = positions[i];

         rl_tree_node* right = rl_tree_node_create(tree);
         if (child != -1) {
            right->children = malloc(sizeof(long) * (tree->max_size + 1));
         }

         if (pos < tree->max_size / 2) {
            if (child != -1) {
               memmove(&node->children[pos + 2], &node->children[pos + 1], sizeof(void*) * (tree->max_size / 2 - 1 - pos));
               node->children[pos + 1] = child;
               memmove(right->children, &node->children[tree->max_size / 2], sizeof(void*) * tree->max_size / 2);
            }
            memmove(&node->scores[pos + 1], &node->scores[pos], sizeof(void*) * (tree->max_size / 2 - 1 - pos));
            node->scores[pos] = score;
            score = node->scores[tree->max_size / 2 - 1];
            memmove(right->scores, &node->scores[tree->max_size / 2], sizeof(void*) * tree->max_size / 2);
         }

         if (pos == tree->max_size / 2) {
            if (child != -1) {
               memmove(right->children, &node->children[tree->max_size / 2], sizeof(void*) * tree->max_size / 2);
            }
            memmove(right->scores, &node->scores[tree->max_size / 2], sizeof(void*) * tree->max_size / 2);
         }

         if (pos > tree->max_size / 2) {
            if (child != -1) {
               memmove(right->children, &node->children[tree->max_size / 2 + 1], sizeof(void*) * (pos - tree->max_size / 2));
               right->children[pos - tree->max_size / 2] = child;
               memmove(&right->children[pos - tree->max_size / 2 + 1], &node->children[tree->max_size / 2 + 1], sizeof(void*) * (tree->max_size - pos));
            }
            memmove(right->scores, &node->scores[tree->max_size / 2], sizeof(void*) * (pos - tree->max_size / 2 - 1));
            right->scores[pos - tree->max_size / 2 - 1] = score;
            memmove(&right->scores[pos - tree->max_size / 2], &node->scores[pos], sizeof(void*) * (tree->max_size - pos));
            score = node->scores[tree->max_size / 2];
         }

         node->size = right->size = tree->max_size / 2;
         child = tree->accessor->setter(tree, right);
      }
   }
   if (score) {
      node = rl_tree_node_create(tree);
      node->size = 1;
      node->scores[0] = score;
      node->children = malloc(sizeof(long) * (tree->max_size + 1));
      node->children[0] = tree->accessor->setter(tree, tree->root);
      node->children[1] = child;
      tree->root = node;
      tree->height++;
      tree->accessor->setter(tree, tree->root);
   }

cleanup:
   free(nodes);
   free(positions);

   return retval;
}

void rl_print_node(rl_tree* tree, rl_tree_node* node, long level) {
   long i, j;
   if (node->children) {
      rl_print_node(tree, (rl_tree_node*)tree->accessor->getter(tree, node->children[i]), level + 1);
   }
   char* score;
   int size;
   for (i = 0; i < node->size; i++) {
      for (j = 0; j < level; j++) {
         putchar('=');
      }
      tree->type->formatter(node->scores[i], &score, &size);
      fwrite(score, sizeof(char), size, stdout);
      free(score);
      printf("\n");
      if (node->children) {
         rl_print_node(tree, (rl_tree_node*)tree->accessor->getter(tree, node->children[i + 1]), level + 1);
      }
   }
}

void rl_print_tree(rl_tree* tree) {
   printf("-------\n");
   rl_print_node(tree, tree->root, 1);
   printf("-------\n");
}