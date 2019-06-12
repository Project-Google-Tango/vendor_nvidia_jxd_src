/*
 * Copyright (c) 2011-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_VERSION_H
#define INCLUDED_VERSION_H


// @Major:  Need to be changed when compatibility breaks between nvflash, nvsbktool
//          and nv3pserver
// @Minor:  Need to be changed when significant amount of change is done
// @Branch: Used to store the branch related information.

// nvflash version

#define NVFLASH_VER 4.13.0000

#define STRING2(s) #s
#define STRING(s) STRING2(s)
#define NVFLASH_VERSION STRING(NVFLASH_VER)

#endif //INCLUDED_VERSION_H

