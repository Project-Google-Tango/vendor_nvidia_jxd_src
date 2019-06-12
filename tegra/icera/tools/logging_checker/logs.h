/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/


#ifdef ANDROID_HOST_BUILD
    #include <stdio.h>
    #include <stdlib.h>
    #define ALOGE(str, ...) printf("E/LGCK: " str "\n", ## __VA_ARGS__)
    #define ALOGW(str, ...) printf("W/LGCK: " str "\n", ## __VA_ARGS__)
    #define ALOGI(str, ...) printf("I/LGCK: " str "\n", ## __VA_ARGS__)
#elif (defined ANDROID)
    #define LOG_TAG "LGCK"
    #include <utils/Log.h>
#else
    #include <stdio.h>
    #include <stdlib.h>
    #define ALOGE(str, ...) printf("E/LGCK: " str "\n", __VA_ARGS__)
    #define ALOGW(str, ...) printf("W/LGCK: " str "\n", __VA_ARGS__)
    #define ALOGI(str, ...) printf("I/LGCK: " str "\n", __VA_ARGS__)
#endif
