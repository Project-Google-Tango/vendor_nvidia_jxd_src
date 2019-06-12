/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NCT_INCLUDED_H
#define NCT_INCLUDED_H

#include "nvnct.h"
#include "nvaboot.h"

/* NvNct Util */
NvU32   NvNctGetTID(void);
NvError NvNctEncryptData(NvU32 *src, NvU32 *dst, NvU32 length);
NvU32   NvNctEncryptU32(NvU32 buf);
NvError NvNctVerifyCheckSum(nct_entry_type *entry);

/* NvNct API */
NvError NvNctInitMutex(void);
NvError NvNctReadItem(NvU32 index, nct_item_type *buf);
NvError NvNctWriteItem(NvU32 index, nct_item_type *buf);
NvU32 NvNctGetItemType(NvU32 index);

/* NvNct Items */
NvError NvNctBuildDefaultPart(nct_part_head_type *Header);

/* NvNct Main */
NvError NvNctCheckCRC(NvU32 baseAddr, NvU32 validLen);
NvError NvNctCopyNCTtoNCK(NvU32 baseAddr, NvU32 validLen);
NvBool  NvNctIsInit(void);
NvError NvNctInitPart(void);
NvError NvNctInitNCKPart(NvAbootHandle hAboot);
NvError NvNctUpdatePartHeader(nct_part_head_type *Header);
NvError NvNctUpdateRevision(NvU32 revision);

#endif
