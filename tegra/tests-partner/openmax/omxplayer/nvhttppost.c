#include "nvutil.h"
#include "nvhttppost.h"
#include "../nvmm/include/nvmm_sock.h"

#include "nvos.h"
#define MAX_SIZE 4096

NvError NvPOSTCallBack(char * postResponse,NvS64 responseSize)
{
    NvError   Status = NvSuccess;
    NvDrmContextHandle pDrmContext;
    char drmdllname[256];
    NvOsLibraryHandle drmdllhandle;
    NvDrmInterface drm_Interface;

    drm_Interface.pNvDrmCreateContext = NULL;
    drm_Interface.pNvDrmProcessLicenseResponse = NULL;
     drm_Interface.pNvDrmDestroyContext = NULL;
     Status =  NvOsGetConfigString("DrmLib.AsfDrm", drmdllname, 256);

     if(Status == NvSuccess)
     {
            Status = NvOsLibraryLoad((const char *)drmdllname, &drmdllhandle);

        if(Status == NvSuccess)
        {
            drm_Interface.pNvDrmCreateContext =(NvError (*)(NvDrmContextHandle *))
            NvOsLibraryGetSymbol(drmdllhandle,"NvDrmCreateContext");
            drm_Interface.pNvDrmDestroyContext=(NvError (*)(NvDrmContextHandle ))
            NvOsLibraryGetSymbol(drmdllhandle,"NvDrmDestroyContext");
            drm_Interface.pNvDrmProcessLicenseResponse=(NvError (*)(NvDrmContextHandle,
            pfnLicenseCallback, void*,
            NvU8 *, NvU32
            ))
            NvOsLibraryGetSymbol(drmdllhandle,"NvDrmProcessLicenseResponse");
        }

           if(drm_Interface.pNvDrmCreateContext != NULL)
              Status = drm_Interface.pNvDrmCreateContext (&pDrmContext);
           if(drm_Interface.pNvDrmProcessLicenseResponse != NULL)
           {
               if ((Status = drm_Interface.pNvDrmProcessLicenseResponse (
                   pDrmContext, 0, 0,(NvU8 *)postResponse,(NvU32)responseSize)) != NvSuccess)
               {
                  NvOsDebugPrintf("\n DRM_MGR_ProcessLicenseResponse() failed status ->%x\n",Status);
                  return Status;
               }
           }
           if(drm_Interface.pNvDrmDestroyContext != NULL)
              drm_Interface.pNvDrmDestroyContext(pDrmContext);

    }
    return Status;
}

NvError NvHttpClient(NvU16  *pwszUrl,NvU32 cchUrl, NvU8   *pszChallenge,NvU32 cchChallenge)
{
    NvError   Status = NvSuccess;

    char *newurl;
    char *challengeBuff = NULL;
    char challenge[11] = "challenge=";

    size_t utf8Length;

   

   challengeBuff = (char *)NvOsAlloc(cchChallenge + NvOsStrlen(challenge));
   if(challengeBuff == NULL)
         goto error;
  NvOsMemcpy(challengeBuff, &challenge, NvOsStrlen(challenge));
  NvOsMemcpy(challengeBuff + NvOsStrlen(challenge), pszChallenge, cchChallenge);
    newurl = NvOsAlloc(MAX_SIZE);
    NvOsMemset(newurl, 0, MAX_SIZE);

    utf8Length = NvUStrlConvertCodePage(NULL, 0, NvOsCodePage_Utf8,
                       pwszUrl, cchUrl, NvOsCodePage_Utf16);

    (void)NvUStrlConvertCodePage(newurl, utf8Length, NvOsCodePage_Utf8,
     pwszUrl, cchUrl, NvOsCodePage_Utf16);

    Status = NvMMSockPOSTHTTPFile(newurl,utf8Length,challengeBuff,cchChallenge + NvOsStrlen(challenge),NvPOSTCallBack);
    NvOsFree(challengeBuff);
    challengeBuff = NULL;
    NvOsFree(newurl);
    newurl = NULL;

    return Status;

error:

    Status = NvError_ResourceError;
    return Status;
}
