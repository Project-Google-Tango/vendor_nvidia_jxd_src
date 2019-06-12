#ifndef __NvCameraJDS_H
#define __NvCameraJDS_H

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nvcommon.h"
#include "nvos.h"
#include "nverror.h"

#define NVCAMERA_JDS_MAX_ARRAY_LENGTH 1024
#define NVCAMERA_JDS_MAKERNOTE_STRING_SIZE 2048
#define NVCAMERA_JDS_VALUE_VEC_SIZE 10
#define NVCAMERA_JDS_MAX_NAME_LENGTH 256

typedef struct JSONRec JSON;

enum Type
{
    SUBTREE,
    CHAR,
    NVS8,
    NVU8,
    NVS16,
    NVU16,
    NVS32,
    NVU32,
    NVF32,
    NVS64,
    NVU64,
    NVF64,
    NVBOOL,

    CHARPTR,
    NVS8PTR,
    NVU8PTR,
    NVS16PTR,
    NVU16PTR,
    NVS32PTR,
    NVU32PTR,
    NVF32PTR,
    NVS64PTR,
    NVU64PTR,
    NVF64PTR,
    NVBOOLPTR,
};

typedef struct JSONValue
{
    enum Type valueType;
    size_t size;
    union
    {
        JSON* subtree;

        char  chr;
        NvS8  s8;
        NvU8  u8;
        NvS16 s16;
        NvU16 u16;
        NvS32 s32;
        NvU32 u32;
        NvF32 f32;
        NvS64 s64;
        NvU64 u64;
        NvF64 f64;
        NvBool nvBool;

        char* chrPtr;
        NvS8*  s8Ptr;
        NvU8*  u8Ptr;
        NvS16* s16Ptr;
        NvU16* u16Ptr;
        NvS32* s32Ptr;
        NvU32* u32Ptr;
        NvF32* f32Ptr;
        NvS64* s64Ptr;
        NvU64* u64Ptr;
        NvF64* f64Ptr;
        NvBool* nvBoolPtr;
    };
} JSONValue;

struct JSONRec
{
    char *name;
    JSONValue value;
    size_t valueSize;
};

typedef struct NvCameraJDSRec
{
    size_t size;
    int length;
    char *makernoteString;
    JSON *root;
} NvCameraJDS;

NvError NvCameraJDSInit(NvCameraJDS *db);
void NvCameraJDSCleanup(NvCameraJDS* db);

NvError NvCameraJDSInsertNvS32(NvCameraJDS* db, const char *name, NvS32 value);
NvError NvCameraJDSInsertNvU32(NvCameraJDS* db, const char *name, NvU32 value);
NvError NvCameraJDSInsertNvF32(NvCameraJDS* db, const char *name, NvF32 value);
NvError NvCameraJDSInsertNvF64(NvCameraJDS* db, const char *name, NvF64 value);
NvError NvCameraJDSInsertNvS16(NvCameraJDS* db, const char *name, NvS16 value);
NvError NvCameraJDSInsertNvU16(NvCameraJDS* db, const char *name, NvU16 value);
NvError NvCameraJDSInsertNvS8(NvCameraJDS* db, const char *name, NvS8 value);
NvError NvCameraJDSInsertNvU8(NvCameraJDS* db, const char *name, NvU8 value);
NvError NvCameraJDSInsertNvBool(NvCameraJDS* db, const char *name, NvBool value);
NvError NvCameraJDSInsertint(NvCameraJDS* db, const char *name, int value);
NvError NvCameraJDSInsertchar(NvCameraJDS* db, const char *name, char value);
NvError NvCameraJDSInsertfloat(NvCameraJDS* db, const char *name, float value);
NvError NvCameraJDSInsertNvS64(NvCameraJDS* db, const char *name, NvS64 value);
NvError NvCameraJDSInsertNvU64(NvCameraJDS* db, const char *name, NvU64 value);

NvError NvCameraJDSInsertNvS32Array(NvCameraJDS* db, const char *name, NvS32* value, size_t size);
NvError NvCameraJDSInsertNvU32Array(NvCameraJDS* db, const char *name, NvU32* value, size_t size);
NvError NvCameraJDSInsertNvF32Array(NvCameraJDS* db, const char *name, NvF32* value, size_t size);
NvError NvCameraJDSInsertNvF64Array(NvCameraJDS* db, const char *name, NvF64* value, size_t size);
NvError NvCameraJDSInsertNvS16Array(NvCameraJDS* db, const char *name, NvS16* value, size_t size);
NvError NvCameraJDSInsertNvU16Array(NvCameraJDS* db, const char *name, NvU16* value, size_t size);
NvError NvCameraJDSInsertNvS8Array(NvCameraJDS* db, const char *name, NvS8* value, size_t size);
NvError NvCameraJDSInsertNvU8Array(NvCameraJDS* db, const char *name, NvU8* value, size_t size);
NvError NvCameraJDSInsertNvBoolArray(NvCameraJDS* db, const char *name, NvBool* value, size_t size);
NvError NvCameraJDSInsertintArray(NvCameraJDS* db, const char *name, int* value, size_t size);
NvError NvCameraJDSInsertcharArray(NvCameraJDS* db, const char *name, char* value, size_t size);
NvError NvCameraJDSInsertfloatArray(NvCameraJDS* db, const char *name, float* value, size_t size);
NvError NvCameraJDSInsertNvS64Array(NvCameraJDS* db, const char *name, NvS64* value, size_t size);
NvError NvCameraJDSInsertNvU64Array(NvCameraJDS* db, const char *name, NvU64* value, size_t size);

void NvCameraJDSSerializeDB(NvCameraJDS* db);

#endif
