
typedef struct NvTioFileCacheRec
{
    char *Name;
    char *Start;
    NvU32 Length;
    NvU32 padding;
} NvTioFileCache;

extern NvTioFileCache g_NvTioFileCache;
