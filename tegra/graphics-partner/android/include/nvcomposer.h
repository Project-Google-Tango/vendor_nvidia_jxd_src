/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef NVCOMPOSER_H
#define NVCOMPOSER_H

#include "hardware/hwcomposer.h"
#include "nvgr.h"

#define NVCOMPOSER_MAX_LAYERS 32

/**
 * Defines important information that affects the behaviour of composition.
 */
typedef enum {
    /**
     * Optimisation flag to inform the composer that the layer configuration
     * of the contents has changed. If this is not set then the composer can
     * assume that only the buffer contents have changed since the last frame,
     * and so certain optimisations can be performed.
     */
    NVCOMPOSER_FLAG_GEOMETRY_CHANGED = (1<<0),

    /**
     * Property flag to inform the composer that one or more of the input
     * buffers is located in VPR memory.
     */
    NVCOMPOSER_FLAG_PROTECTED = (1<<1),

    /**
     * Optimisation flag to inform the composer that the contents represent
     * the last pass of composition, meaning that the target buffer is the
     * bottom-most layer and will not subsequently be blending on top of any
     * other layers. The composer can use this information to optimise how it
     * writes the alpha component to the target buffer.
     */
    NVCOMPOSER_FLAG_LAST_PASS = (1<<2),

    /**
     * Optimisation flag to inform the composer that target buffer is coming
     * from a different buffer queue than the last frame. If the composer is
     * caching target buffer information, then this flag is useful for
     * determining when that cache needs to be purged.
     */
    NVCOMPOSER_FLAG_TARGET_CHANGED = (1<<3)
} nvcomposer_flags_t;

/**
 * Defines the capabilites of a composer.
 */
typedef enum {
    /**
     * Composer can transform input layers.
     */
    NVCOMPOSER_CAP_TRANSFORM = (1<<0),

    /**
     * Composer can access buffers in VPR memory.
     */
    NVCOMPOSER_CAP_PROTECT = (1<<1),

    /**
     * Composer can read compressed buffers.
     */
    NVCOMPOSER_CAP_DECOMPRESS = (1<<2),

    /**
     * Composer can write to a buffer in pitch linear layout.
     */
    NVCOMPOSER_CAP_PITCH_LINEAR = (1<<3)
} nvcomposer_caps_t;

/**
 * Defines the layer configuration of the composition contents.
 */
typedef struct nvcomposer_contents {
    /**
     * Number of input layers to compose.
     */
    int numLayers;

    /**
     * Minimum and maximum scale factor used by the input layers. Although this
     * is not neccesary for carrying out composition, it is useful to store this
     * information for validation against a composer's capabilites.
     */
    float minScale;
    float maxScale;

    /**
     * List of input layers to compose.
     *
     * This provides the composer with information about the configuration of
     * each layer, its input buffer, and synchronisation details. The composer
     * must use the acquireFenceFd of each layer to ensure composition does not
     * start reading from the buffer until that sync object has been signalled.
     *
     * However the composer does NOT take ownership of that sync object, so
     * the client is still responsible for closing it after calling
     * nvcomposer_t.set.
     *
     * Likewise the composer will NOT update the releaseFenceFd of the layer, so
     * after calling nvcomposer_t.set the client is responsible for updating
     * each layer with the fence returned in nvcomposer_target_t.releaseFenceFd.
     */
    hwc_layer_1_t layers[NVCOMPOSER_MAX_LAYERS];

    /**
     * Destination rectangle to clip the composited layers against.
     */
    NvRect clip;

    /**
     * Bitmask of nv_composer_flags_t.
     */
    unsigned flags;
} nvcomposer_contents_t;

/**
 * Defines the properties of the composition target.
 */
typedef struct nvcomposer_target {
    /**
     * Destination buffer for composition.
     */
    buffer_handle_t surface;

    /**
     * The acquireFenceFd sync object is used to ensure composition does not
     * start writing to the target surface until this sync object has been
     * signalled. The composer takes ownership of the sync object and will close
     * it when no longer needed.
     */
    int acquireFenceFd;

    /**
     * The releaseFenceFd sync object is used to return a sync object to the
     * client. It is this sync object that will be signalled once composition is
     * complete. The client takes ownership of this sync object and so is
     * responsible for closing it.
     */
    int releaseFenceFd;

    /**
     * Bitmask of nv_composer_flags_t.
     */
    unsigned flags;
} nvcomposer_target_t;

typedef struct nvcomposer nvcomposer_t;

/**
 * Tells the composer to release all resources and close.
 *
 * After calling this function, the client must no longer attempt to access
 * the nvcomposer_t instance as it may have been freed.
 */
typedef void (* nvcomposer_close_t)(nvcomposer_t *composer);

/**
 * Tells the composer to prepare for compositing the specified contents.
 *
 * A return value of 0 indicates success.
 * Any other value indicates there was an error, and that its not possible to
 * composite the contents with this composer.
 */
typedef int (* nvcomposer_prepare_t)(nvcomposer_t *composer,
                                     const nvcomposer_contents_t *contents);

/**
 * Tells the composer to carry out composition of the specified contents and
 * output the results to the specified target.
 *
 * A return value of 0 indicates success.
 * Any other value indicates there was an error, and that composition was not
 * done.
 */
typedef int (* nvcomposer_set_t)(nvcomposer_t *composer,
                                 const nvcomposer_contents_t *contents,
                                 nvcomposer_target_t *target);

/**
 * Defines the interface to an internal NVIDIA composer module.
 * The client does not create instances of this structure, but instead retrieves
 * a instance by calling the appropriate composer library creation function, for
 * example nvviccomposer_create() or glc_get().
 */
struct nvcomposer {
    /**
     * Identity string of the composer.
     */
    const char *name;

    /**
     * Bitmask of nv_composer_caps_t.
     */
    unsigned caps;

    /**
     * Maximum number of layer that can be composited.
     */
    int maxLayers;

    /**
     * Minimum and maximum scale factor that can be applied to an input layer.
     */
    float minScale;
    float maxScale;

    /**
     * Functions to operate the composer.
     * Depending on its implementation, the close function may be NULL in which
     * case the client needs to check before calling them.
     */
    nvcomposer_close_t   close;
    nvcomposer_prepare_t prepare;
    nvcomposer_set_t     set;
};

#endif /* NVCOMPOSER_H */
