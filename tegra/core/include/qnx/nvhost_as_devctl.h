/*
 * Tegra Host Address Space Driver
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVHOST_AS_DEVCTL_H
#define __NVHOST_AS_DEVCTL_H

#include <qnx/rmos_types.h>
#include <devctl.h>

/*
 * /dev/nvhost-as-* devices
 *
 * Opening a '/dev/nvhost-as-<module_name>' device node creates a new address
 * space.  nvhost channels (for the same module) can then be bound to such an
 * address space to define the addresses it has access to.
 *
 * Once a nvhost channel has been bound to an address space it cannot be
 * unbound.  There is no support for allowing an nvhost channel to change from
 * one address space to another (or from one to none).
 *
 * As long as there is an open device file to the address space, or any bound
 * nvhost channels it will be valid.  Once all references to the address space
 * are removed the address space is deleted.
 *
 */


/*
 * Allocating an address space range:
 *
 * Address ranges created with this devctl are reserved for later use with
 * fixed-address buffer mappings.
 *
 * If _FLAGS_FIXED_OFFSET is specified then the new range starts at the 'offset'
 * given.  Otherwise the address returned is chosen to be a multiple of 'align.'
 *
 */
struct nvhost_as_alloc_space_args {
    rmos_u32 pages;     /* in, pages */
    rmos_u32 page_size; /* in, bytes */
    rmos_u32 flags;     /* in */
#define NVHOST_AS_ALLOC_SPACE_FLAGS_FIXED_OFFSET 0x1
    union {
        rmos_u64 offset; /* inout, byte address valid iff _FIXED_OFFSET */
        rmos_u64 align;  /* in, alignment multiple (0:={1 or n/a}) */
    } o_a;
};

/*
 * Releasing an address space range:
 *
 * The previously allocated region starting at 'offset' is freed.  If there are
 * any buffers currently mapped inside the region the devctl will fail.
 */
struct nvhost_as_free_space_args {
    rmos_u64 offset;    /* in, byte address */
    rmos_u32 pages;     /* in, pages */
    rmos_u32 page_size; /* in, bytes */
};

/*
 * Binding a nvhost channel to an address space:
 *
 * A channel must be bound to an address space before allocating a gpfifo
 * in nvhost.  The 'channel_fd' given here is the fd used to allocate the
 * channel.  Once a channel has been bound to an address space it cannot
 * be unbound (except for when the channel is destroyed).
 */
struct nvhost_as_bind_channel_args {
    rmos_u32 channel_fd; /* in */
};

/*
 * Mapping nvmap buffers into an address space:
 *
 * The start address is the 'offset' given if _FIXED_OFFSET is specified.
 * Otherwise the address returned is a multiple of 'align.'
 *
 * If 'page_size' is set to 0 the nvmap buffer's allocation alignment/sizing
 * will be used to determine the page size (largest possible).  The page size
 * chosen will be returned back to the caller in the 'page_size' parameter in
 * that case.
 */
struct nvhost_as_map_buffer_args {
    rmos_u32 flags;          /* in/out */
#define NVHOST_AS_MAP_BUFFER_FLAGS_FIXED_OFFSET        (1 << 0)
#define NVHOST_AS_MAP_BUFFER_FLAGS_CACHEABLE        (1 << 2)
    rmos_u32 nvmap_fd;       /* in */
    rmos_u32 nvmap_handle;   /* in */
    rmos_u32 page_size;      /* inout, 0:= best fit to buffer */
    union {
        rmos_u64 offset; /* inout, byte address valid iff _FIXED_OFFSET */
        rmos_u64 align;  /* in, alignment multiple (0:={1 or n/a})   */
    } o_a;
};

/*
 * Unmapping a buffer:
 *
 * To unmap a previously mapped buffer set 'offset' to the offset returned in
 * the mapping call.  This includes where a buffer has been mapped into a fixed
 * offset of a previously allocated address space range.
 */
struct nvhost_as_unmap_buffer_args {
    rmos_u64 offset; /* in, byte address */
};

#define NVHOST_AS_IOCTL_BIND_CHANNEL    \
    NVHOST_AS_DEVCTL_BIND_CHANNEL
#define NVHOST_AS_IOCTL_MAP_BUFFER      \
    NVHOST_AS_DEVCTL_MAP_BUFFER
#define NVHOST_AS_IOCTL_UNMAP_BUFFER    \
    NVHOST_AS_DEVCTL_UNMAP_BUFFER

#define NVHOST_AS_DEVCTL_BIND_CHANNEL \
    (int)__DIOTF(_DCMD_MISC, 1, struct nvhost_as_bind_channel_args)
#define NVHOST_AS_DEVCTL_ALLOC_SPACE \
    (int)__DIOTF(_DCMD_MISC, 2, struct nvhost_as_alloc_space_args)
#define NVHOST_AS_DEVCTL_FREE_SPACE \
    (int)__DIOTF(_DCMD_MISC, 3, struct nvhost_as_free_space_args)
#define NVHOST_AS_DEVCTL_MAP_BUFFER \
    (int)__DIOTF(_DCMD_MISC, 4, struct nvhost_as_map_buffer_args)
#define NVHOST_AS_DEVCTL_UNMAP_BUFFER \
    (int)__DIOTF(_DCMD_MISC, 5, struct nvhost_as_unmap_buffer_args)

#define NVHOST_AS_DEVCTL_MAX_ARG_SIZE    \
    sizeof(struct nvhost_as_map_buffer_args)


#endif /* __NVHOST_AS_DEVCTL_H */
