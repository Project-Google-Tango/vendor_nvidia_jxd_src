/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LATINIME_BLOOM_FILTER_H
#define LATINIME_BLOOM_FILTER_H

#include <stdint.h>

#include "defines.h"

namespace latinime {

// TODO: uint32_t position
static inline void setInFilter(uint8_t *filter, const int32_t position) {
    const uint32_t bucket = static_cast<uint32_t>(position % BIGRAM_FILTER_MODULO);
    filter[bucket >> 3] |= static_cast<uint8_t>(1 << (bucket & 0x7));
}

// TODO: uint32_t position
static inline bool isInFilter(const uint8_t *filter, const int32_t position) {
    const uint32_t bucket = static_cast<uint32_t>(position % BIGRAM_FILTER_MODULO);
    return filter[bucket >> 3] & static_cast<uint8_t>(1 << (bucket & 0x7));
}
} // namespace latinime
#endif // LATINIME_BLOOM_FILTER_H
