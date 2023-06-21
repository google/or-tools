// Copyright 2010-2022 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hopcroft_karp.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __KERNEL__
#define ALLOC(bytes, cpu) \
        kzalloc_node(bytes, GFP_KERNEL, cpu_to_node(cpu))
#define FREE(ptr) kfree(ptr)
#else
#define ALLOC(bytes, cpu) malloc(bytes)
#define FREE(ptr) free(ptr)
#define WARN_ON_ONCE(exp) (assert(!(exp)), 0)
#endif

/*
 * Hopcroft Karp Algorithm Implementation: Start
 */

/*
 * The 'cpu' argument is used if you are in an environment where you want
 * memory allocation to be pinned to a specific CPU (in particular for NUMA
 * architectures); currently this is only used if the __KERNEL__ macro is
 * defined; if it's not defined, the argument is ignored.
 */
void hopcroft_karp_state_init(int u_size, int v_size, int cpu,
                              struct hopcroft_karp_state* hk_state)
{
    int bytes;
    int* memory;

    hk_state->graph.u_size = u_size;
    hk_state->graph.v_size = v_size;

    /*
     * Memory Required
     *
     * hk_state->pair_u:         u_size * sizeof(int)
     * hk_state->pair_v:         v_size * sizeof(int)
     * hk_state->distance:       (1 + u_size) * sizeof(int)
     * hk_state->queue_data:     u_size * sizeof(int)
     * hk_state->u_stack:        u_size * sizeof(int)
     * hk_state->v_stack:        u_size * sizeof(int)
     * hk_state->graph.adjacent: u_size * v_size * sizeof(char)
     */
    bytes = u_size * sizeof(int) + v_size * sizeof(int) +
            (1 + u_size) * sizeof(int) + u_size * sizeof(int) +
            u_size * sizeof(int) + u_size * sizeof(int) +
            u_size * v_size * sizeof(char);

    memory = (int*)ALLOC(bytes, cpu);
    if (WARN_ON_ONCE(memory == NULL)) {
        return;
    }

    hk_state->pair_u = memory;
    memory += u_size;

    hk_state->pair_v = memory;
    memory += v_size;

    hk_state->distance = memory;
    memory += (1 + u_size);

    hk_state->queue_data = memory;
    memory += u_size;

    hk_state->u_stack = memory;
    memory += u_size;

    hk_state->v_stack = memory;
    memory += u_size;

    hk_state->graph.adjacent = (char*)memory;
}

void hopcroft_karp_state_free(struct hopcroft_karp_state* hk_state)
{
    FREE(hk_state->pair_u);
    hk_state->pair_u = NULL;
    hk_state->pair_v = NULL;
    hk_state->distance = NULL;
    hk_state->queue_data = NULL;
    hk_state->u_stack = NULL;
    hk_state->v_stack = NULL;
    hk_state->graph.adjacent = NULL;
}

/*
 * Breadth-first search: Separates the vertices of the graph into layers.
 */
static int breadth_first_search(struct hopcroft_karp_state* hk_state)
{
    int u;
    int queue_size = 0, queue_front = 0, queue_index = 0;

    /*
     * The free vertices of U, i.e. those that are paired with the NIL node,
     * are used as the starting vertices and form the first layer of the
     * partition.
     */
    for (u = 0; u < hk_state->graph.u_size; ++u) {
        if (hk_state->pair_u[u] == HK_NIL_NODE) {
            hk_state->distance[1 + u] = 0;
            /* Enqueue u */
            hk_state->queue_data[queue_size] = u;
            queue_size++;
        } else {
            hk_state->distance[1 + u] = INT_MAX;
        }
    }

    /* NIL node */
    hk_state->distance[0] = INT_MAX;

    /*
     * Traversal: Alternate between unmatched edges starting from free U nodes
     * and matched edges starting from paired V nodes. The search terminates
     * at the first level k where one or more free vertices in V are reached.
     */
    while (queue_size > 0) {
        /* Dequeue u */
        u = hk_state->queue_data[queue_front];
        queue_size--;
        queue_front++;
        queue_front = queue_front < hk_state->graph.u_size ? queue_front : 0;

        if (hk_state->distance[1 + u] < hk_state->distance[0]) {
            int v;
            int u_row = u * hk_state->graph.v_size;
            for (v = 0; v < hk_state->graph.v_size; ++v) {
                if (hk_state->graph.adjacent[u_row + v]) {
                    if (hk_state->distance[1 + hk_state->pair_v[v]] == INT_MAX) {
                        hk_state->distance[1 + hk_state->pair_v[v]] =
                          SATURATED_INC(hk_state->distance[1 + u]);
                        /* Enqueue pair_v[v] */
                        queue_index = queue_front + queue_size;
                        queue_index = queue_index < hk_state->graph.u_size ?
                                      queue_index :
                                      queue_index - hk_state->graph.u_size;
                        hk_state->queue_data[queue_index] = hk_state->pair_v[v];
                        queue_size++;
                    }
                }
            }
        }
    }

    return hk_state->distance[0] != INT_MAX;
}

/*
 * Depth-first search: Computes a maximal set of vertex disjoint shortest
 *                     augmenting paths of length k starting from the free
 *                     vertices of V computed by BFS.
 *                     (Iterative implementation)
 */
static int depth_first_search(int u_node, struct hopcroft_karp_state* hk_state)
{
    int stack_length = 1, u_row, u;
    int* v_ptr;

    /* If a free vertex of U is reached, an augmenting path was found. */
    if (u_node == HK_NIL_NODE) {
        return 1;
    }

    if (WARN_ON_ONCE(hk_state->graph.u_size <= 0)) {
        return -1;
    }

    /* For each 'u' node start examining from the first node in the V set */
    memset(hk_state->v_stack, 0, hk_state->graph.u_size * sizeof(int));
    hk_state->u_stack[0] = u_node;

    while (stack_length > 0) {
        /* u = top of stack */
        u = hk_state->u_stack[stack_length - 1];
        u_row = u * hk_state->graph.v_size;
        v_ptr = &(hk_state->v_stack[stack_length - 1]);

        /* For this 'u' node scan the 'v' nodes in the V set */
        for (; *v_ptr < hk_state->graph.v_size; (*v_ptr)++) {
            /* If this 'v' is connected to the current 'u' node */
            if (hk_state->graph.adjacent[u_row + *v_ptr]) {
                /* If it is not more than one hop away */
                if (hk_state->distance[1 + hk_state->pair_v[*v_ptr]]
                        == SATURATED_INC(hk_state->distance[1 + u])) {
                    /* Is this 'v' node paired with a NIL node? */
                    if (hk_state->pair_v[*v_ptr] == HK_NIL_NODE) {
                        /*
                         * A free vertex of U is reached and an augmenting path
                         * was found. Pop the entire stack and return true.
                         */
                        while (stack_length > 0) {
                            stack_length--;
                            /* Adjust pairs */
                            hk_state->pair_v[hk_state->v_stack[stack_length]] =
                                hk_state->u_stack[stack_length];
                            hk_state->pair_u[hk_state->u_stack[stack_length]] =
                                hk_state->v_stack[stack_length];
                        }
                        return 1;
                    } else {
                        /*
                         * In case we need further investigation push
                         * the new 'u' node discovered, in the stack.
                         */
                        hk_state->v_stack[stack_length] = 0;
                        hk_state->u_stack[stack_length] =
                            hk_state->pair_v[*v_ptr];
                        stack_length++;
                    }
                    break;
                }
            }
        }

        if (*v_ptr >= hk_state->graph.v_size) {
            /*
             * The 'u' node's neighbors in V set were fully scanned and
             * nothing interesting was found: pop and continue with the rest.
             */
            hk_state->distance[1 + u] = INT_MAX;
            stack_length--;
        }
    }

    return 0;
}

int hopcroft_karp_matching(struct hopcroft_karp_state* hk_state, int* pairs)
{
    int u, v, matching = 0;

    for (u = 0; u < hk_state->graph.u_size; ++u) {
        hk_state->pair_u[u] = HK_NIL_NODE;
    }
    for (v = 0; v < hk_state->graph.v_size; ++v) {
        hk_state->pair_v[v] = HK_NIL_NODE;
    }

    while (breadth_first_search(hk_state)) {
        for (u = 0; u < hk_state->graph.u_size; ++u) {
            if (hk_state->pair_u[u] == HK_NIL_NODE) {
                if (depth_first_search(u, hk_state)) {
                    ++matching;
                }
            }
        }
    }

    /* Result */
    if (pairs) {
        for (u = 0; u < hk_state->graph.u_size; ++u) {
            pairs[u] = hk_state->pair_u[u];
        }
    }

    return matching;
}
/*
 * Hopcroft Karp Algorithm Implementation: End
 */

#undef SATURATED_INC
#undef ALLOC
#undef FREE

#ifndef __KERNEL__
#undef WARN_ON_ONCE
#endif
