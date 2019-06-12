/* Copyright (c) 2006 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property 
 * and proprietary rights in and to this software, related documentation 
 * and any modifications thereto.  Any use, reproduction, disclosure or 
 * distribution of this software and related documentation without an 
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVXERROR_H
#define NVXERROR_H

typedef enum NVX_ERRORTYPE {
    NVX_Success_OMXReservedLast = 0x10000000-1,  /**< last success enum reserved by OMX before vendor range */
    NVX_SuccessNoThreading,
    NVX_Error_OMXReservedLast = 0x90000000-1,   /**< last error enum reserved by OMX before vendor range */
    NVX_ErrorInsufficientTime,   /**< Not enough time allocated during SchedulerRun */
    NVX_Error_MAX
} NVX_ERRORTYPE;


#define NvxIsError( _e_ )   ( ((_e_) & 0x80000000) != 0)
#define NvxIsSuccess( _e_ ) (!NvxIsError( _e_ ))

/* NvxCheckError(_dest_, _new_)
   Assign _new_ to _dest_ iff _dest_ is a succesful error code
*/
#define NvxCheckError( _dest_, _new_ ) do {     \
   OMX_ERRORTYPE eTemp = _new_;                 \
   if (NvxIsSuccess(_dest_))                    \
       _dest_ = eTemp;                          \
}                                               \
while(0) 

#endif /* NVXERROR_H */
