/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef LIBAUDIO_LIST_H
#define LIBAUDIO_LIST_H

typedef void* list_handle;
typedef void* node_handle;

list_handle list_create(void);
void list_destroy(list_handle h_list);
int list_push_data(list_handle h_list, void *p); /* Push to head */
void list_pop_data(list_handle h_list); /* Pop from tail */
int list_delete_data(list_handle h_list, void *p);
node_handle list_find_data(list_handle h_list, void *p);
void list_delete_node(list_handle h_list, node_handle h_node);

node_handle list_get_head(list_handle h_list);
node_handle list_get_tail(list_handle h_list);
node_handle list_get_next_node(node_handle h_node);
void * list_get_node_data(node_handle h_node);
int list_get_node_count(list_handle h_list);

#endif // LIBAUDIO_LIST_H
