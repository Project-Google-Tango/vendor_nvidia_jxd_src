/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
#ifndef ANDROID_NVCPUD_CONFIG_H
#define ANDROID_NVCPUD_CONFIG_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/String8.h>
#include <utils/Timers.h>

namespace android {
namespace nvcpu {

class Config {
public:
    nsecs_t configRefreshNs;
    bool enabled;
    bool bootCompleted;

    bool checkBootComplete();
    void refresh();

    Config() : //defaults in case string parse fails
        configRefreshNs(45555 * 1000000LL),
        enabled(true),
        bootCompleted(false) {}
private:
    class Property {
    public:
        Property(const char* key,
                const char* def) :
            key(key), def(def)  {}
        void get(char* out) const;
    private:
        String8 key;
        String8 def;
    };

    static const Property refreshMsProp;
    static const Property enabledProp;
    static const Property bootCompleteProp;
};

}; //nvcpu
}; //android

#endif //ANDROID_NVCPUD_CONFIG_H

