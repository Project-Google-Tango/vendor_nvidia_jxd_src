/* This NvMM block is provided as a reference to block writers. As such it is a demonstration of correct semantics
   not a template for the authoring of blocks. Actual implementation may vary. For instance it may create a worker
   thread rather than handle processing synchronously. */

#include "nvmm.h"
#include "nvmm_event.h"
#include "nvos.h"
#include "nvrm_transport.h"

#ifndef INCLUDED_NVMM_AACDEC_H
#define INCLUDED_NVMM_AACDEC_H

NvError
NvMMAacDecOpen(
    NvMMBlockHandle *phBlock,
    NvMMInternalCreationParameters *pParams,
    NvOsSemaphoreHandle semaphore,
    NvMMDoWorkFunction *pDoWorkFunction );

void
NvMMAacDecClose(
    NvMMBlockHandle hBlock,
    NvMMInternalDestructionParameters *pParams);

#endif // INCLUDED_NVMM_AACDEC_H
