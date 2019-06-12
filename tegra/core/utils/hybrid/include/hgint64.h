#ifndef __HGINT64_H
#define __HGINT64_H
/*------------------------------------------------------------------------
 *
 * Hybrid Compatibility Header
 * -----------------------------------------
 *
 * (C) 2002-2003 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * Description:         Portable routines for manipulation of 64-bit integers
 *
 * $Id: //hybrid/libs/hg/main/hgInt64.h#1 $
 * $Date: 2005/11/08 $
 * $Author: janne $ *
 *----------------------------------------------------------------------*/

#if !defined (__HGINT32_H)
#       include "hgint32.h"
#endif

/*lint -save -e701 -e702 */                             /* suppress some LINT warnings          */
/*@-shiftimplementation -shiftnegative@*/

#if (HG_COMPILER == HG_COMPILER_MSC)
#       pragma warning (disable:4035)           /* don't whine about missing return values (in assembler routines) */
#endif

/* select proper implementation */
#if defined (HG_64_BIT_INTS)
#       include "hgint64native.inl"
#else
#       include "hgint64emulated.inl"
#endif

#if (HG_COMPILER == HG_COMPILER_MSC)
#       pragma warning (default:4035)           /* don't whine about missing return values */
#endif

/*lint -restore */
/*@=shiftimplementation =shiftnegative@*/


/*----------------------------------------------------------------------*/
#endif /* __HGINT64_H */


