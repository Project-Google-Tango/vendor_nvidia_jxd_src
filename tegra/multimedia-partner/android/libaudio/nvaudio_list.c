/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define LOG_TAG "nvaudio_list"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <stdlib.h>
#include <cutils/log.h>
#include "nvaudio_list.h"

struct node {
    struct node *next;
    void* data;
};

struct list {
    struct node* head;
    struct node* tail;
    unsigned int len;
};

list_handle list_create()
{
    struct list *l = malloc(sizeof(*l));
    if (!l) {
        ALOGE("Failed to allocated list");
        return 0;
    }
    memset(l, 0, sizeof(*l));
    return l;
}

void list_destroy(list_handle h_list)
{
    struct list *l = h_list;
    struct node *n, *m;
    if (!l)
        return;

    n = l->head;
    while (n) {
        m = n;
        n = n->next;
        free(m);
    }
    free(l);
}

/* Push to tail */
int list_push_data(list_handle h_list, void *p)
{
    struct list *l = h_list;
    struct node *n = NULL;

    if (!l)
        return -ENOENT;

    n = malloc(sizeof(*n));
    if (!n) {
        ALOGE("Failed to allocated node");
        return -ENOMEM;
    }

    n->next = NULL;
    n->data = p;

    if (!l->head)
        l->head = l->tail = n;
    else {
        l->tail->next = n;
        l->tail = n;
    }
    l->len++;

    return 0;
}

/* Pop from tail */
void list_pop_data(list_handle h_list)
{
    struct list *l = h_list;
    struct node *n, *m;
    int i;

    if (!l || !l->len)
        return;

    l->len--;
    if (!l->len) {
        m = l->head;
        l->head = l->tail = NULL;
    } else {
        n = l->head;
        while (n->next != l->tail)
            n = n->next;
        l->tail = n;
        m = n->next;
        n->next = NULL;
    }

    if (m)
        free(m);
}

int list_delete_data(list_handle h_list, void *p)
{
    struct list *l = h_list;
    struct node *n, *m;

    if (!l || !l->head)
        return -EINVAL;

    n = l->head;
    if (n->data == p) {
        l->head = n->next;
        l->len--;
        if (!l->len)
            l->tail = NULL;
        free(n);
        return 0;
    }
    while (n->next) {
        if (n->next->data == p) {
            m = n->next;
            n->next = n->next->next;
            l->len--;
            if (!n->next)
                l->tail = n;
            free(m);
            return 0;
        }
        n = n->next;
    }
    return -EINVAL;
}

node_handle list_find_data(list_handle h_list, void *p)
{
    struct list *l = h_list;
    struct node *n;

    if (!l)
        return 0;

    n = l->head;
    while (n) {
        if (n->data == p)
            return n;
        n = n->next;
    }

    return 0;
}

inline void list_delete_node(list_handle h_list, node_handle h_node)
{
    struct list *l = h_list;
    struct node *n = h_node, *m;

    if (!l || !n)
        return;

    if (n == l->tail) {
        struct node *n_i = l->head;

        while (n_i->next != n)
            n_i = n_i->next;

        l->tail = n_i;
        n_i->next = NULL;
        l->len--;
        free(n);
    } else {
        n->data = n->next->data;
        m = n->next;
        n->next = n->next->next;
        l->len--;
        free(m);
    }
}

inline node_handle list_get_head(list_handle h_list)
{
    return ((struct list*)h_list)->head;
}

inline node_handle list_get_tail(list_handle h_list)
{
    return ((struct list*)h_list)->tail;
}

inline node_handle list_get_next_node(node_handle h_node)
{
    return ((struct node*)h_node)->next;
}

inline void * list_get_node_data(node_handle h_node)
{
    return ((struct node*)h_node)->data;
}

inline int list_get_node_count(list_handle h_list)
{
    return ((struct list*)h_list)->len;
}
