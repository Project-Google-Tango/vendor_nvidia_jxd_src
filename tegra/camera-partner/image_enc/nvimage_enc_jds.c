#include "nvimage_enc_jds.h"

#include <string.h>
#define RESIZE_THRESHOLD 0.8
#define RESIZE_FACTOR 2


static void NvCameraJDSAppendStrToMakerNote(NvCameraJDS *db, const char *str)
{
    int lengthOfNewStr = strlen(str) + db->length;

    if (lengthOfNewStr + 1 >
        RESIZE_THRESHOLD * db->size)
    {
        while (lengthOfNewStr + 1 >
            RESIZE_THRESHOLD * db->size)
        {
            db->size *= RESIZE_FACTOR;
        }
        db->makernoteString = NvOsRealloc(db->makernoteString, db->size * sizeof(char));
    }
    strncpy(db->makernoteString + db->length, str, lengthOfNewStr - db->length);
    db->length = lengthOfNewStr;
    db->makernoteString[db->length + 1] = '\0';
}


static void NvCameraJDSSerializePrimitive(NvCameraJDS *db, JSON* currentVertex)
{
    char buf[NVCAMERA_JDS_MAX_NAME_LENGTH];
    memset(buf, 0, NVCAMERA_JDS_MAX_NAME_LENGTH * sizeof(char));
    switch (currentVertex->value.valueType)
    {
        case CHAR:
            /* if value is NULL, sting gets terminated abnormally */
            if (currentVertex->value.chr)
                sprintf(buf, "\"%s\":\"%c\"", currentVertex->name, currentVertex->value.chr);
            else
                sprintf(buf, "\"%s\":\"\"", currentVertex->name);
            break;
        case NVS8:
            sprintf(buf, "\"%s\":%hhd", currentVertex->name, currentVertex->value.s8);
            break;
        case NVU8:
            sprintf(buf, "\"%s\":%hhu", currentVertex->name, currentVertex->value.u8);
            break;
        case NVS16:
            sprintf(buf, "\"%s\":%hd", currentVertex->name, currentVertex->value.s16);
            break;
        case NVU16:
            sprintf(buf, "\"%s\":%hd", currentVertex->name, currentVertex->value.u16);
            break;
        case NVS32:
            sprintf(buf, "\"%s\":%d", currentVertex->name, currentVertex->value.s32);
            break;
        case NVU32:
            sprintf(buf, "\"%s\":%u", currentVertex->name, currentVertex->value.u32);
            break;
        case NVF32:
            sprintf(buf, "\"%s\":%f", currentVertex->name, currentVertex->value.f32);
            break;
        case NVS64:
            sprintf(buf, "\"%s\":%lld", currentVertex->name, currentVertex->value.s64);
            break;
        case NVU64:
            sprintf(buf, "\"%s\":%llu", currentVertex->name, currentVertex->value.u64);
            break;
        case NVF64:
            sprintf(buf, "\"%s\":%f", currentVertex->name, currentVertex->value.f64);
            break;
        case NVBOOL:
            sprintf(buf, "\"%s\":%d", currentVertex->name, currentVertex->value.nvBool);
            break;
        default:
            break;
    }
    NvCameraJDSAppendStrToMakerNote(db, buf);
}


static void NvCameraJDSSerializeArray(NvCameraJDS *db, JSON* currentVertex)
{
    NvU32 i;
    int bytesWritten = 0;
    char buf[NVCAMERA_JDS_MAX_ARRAY_LENGTH];
    memset(buf, 0, NVCAMERA_JDS_MAX_ARRAY_LENGTH);
    switch (currentVertex->value.valueType)
    {
        case CHARPTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":\"", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(char); ++i)
            {
                if (currentVertex->value.chrPtr[i] == 0)
                {
                    break;
                }
                buf[bytesWritten++] = currentVertex->value.chrPtr[i];
            }
            buf[bytesWritten] = '\"';
            NvCameraJDSAppendStrToMakerNote(db, buf);
            return;
        case NVS8PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvS8); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%hhd,", currentVertex->value.s8Ptr[i]);
            }
            break;
        case NVU8PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\"[:", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvU8); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%hhu,", currentVertex->value.u8Ptr[i]);
            }
            break;
        case NVS16PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvS16); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%hd,", currentVertex->value.s16Ptr[i]);
            }
            break;
        case NVU16PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvU16); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%hu,", currentVertex->value.u16Ptr[i]);
            }
            break;
        case NVS32PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvS32); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%d,", currentVertex->value.s32Ptr[i]);
            }
            break;
        case NVU32PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvU32); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%u,", currentVertex->value.u32Ptr[i]);
            }
            break;
        case NVF32PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvF32); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%f,", currentVertex->value.f32Ptr[i]);
            }
            break;
        case NVS64PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvS64); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%lld,", currentVertex->value.s64Ptr[i]);

            }
            break;
        case NVU64PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvU64); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%llu,", currentVertex->value.u64Ptr[i]);

            }
            break;
        case NVF64PTR:
            bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvF64); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%f,", currentVertex->value.f64Ptr[i]);
            }
            break;
        case NVBOOLPTR:
           bytesWritten += sprintf(buf + bytesWritten, "\"%s\":[", currentVertex->name);
            for (i = 0; i < currentVertex->value.size/sizeof(NvBool); ++i)
            {
                bytesWritten += sprintf(buf + bytesWritten, "%d,", currentVertex->value.nvBoolPtr[i]);

            }
            break;
        default:
            break;
    }
    bytesWritten += sprintf(buf + bytesWritten - 1, "]");
    NvCameraJDSAppendStrToMakerNote(db, buf);
}


static void NvCameraJDSSerializeDBStatic(NvCameraJDS* db, JSON* currentVertex);


static void NvCameraJDSSerializeSubtree(NvCameraJDS *db, JSON* currentVertex)
{
    NvU32 i;
    char buf[NVCAMERA_JDS_MAX_NAME_LENGTH];
    memset(buf, 0, NVCAMERA_JDS_MAX_NAME_LENGTH);
    if (currentVertex->value.valueType == SUBTREE)
    {
        sprintf(buf, "\"%s\":{", (const char*) currentVertex->name);
        NvCameraJDSAppendStrToMakerNote(db, buf);
        for (i = 0; i < currentVertex->valueSize; i++)
        {
            // That's the way we distinguish between child nodes that are actually in use
            // and the ones that and the ones which are allocated in advance -
            // unsed ones do not have the name (it's NULL)
            if (!(currentVertex->value.subtree[i].name))
            {
                break;
            }
            NvCameraJDSSerializeDBStatic(db, &currentVertex->value.subtree[i]);
            NvCameraJDSAppendStrToMakerNote(db, ",");
        }
        db->length--;
        NvCameraJDSAppendStrToMakerNote(db, "}");
    }
}


static void NvCameraJDSSerializeDBStatic(NvCameraJDS* db, JSON* currentVertex)
{
    NvCameraJDSSerializePrimitive(db, currentVertex);
    NvCameraJDSSerializeArray(db, currentVertex);
    NvCameraJDSSerializeSubtree(db, currentVertex);
}


static JSON* NvCameraJDSSearchVertex(JSON *currentVertex, char* name)
{
    NvU32 i;
    if (currentVertex != NULL && currentVertex->value.subtree != NULL)
    {
        for (i = 0; i < currentVertex->valueSize; ++i)
        {
            if (currentVertex->value.subtree[i].name != NULL)
            {
                if (strcmp((currentVertex->value.subtree[i].name), name) == 0)
                {
                    return &(currentVertex->value.subtree[i]);
                }
            }
        }
    }
    return NULL;
}


static void NvCameraJDSFreeJSON(JSON *tree)
{
    NvU32 i;
    if (!tree)
    {
        return;
    }
    switch (tree->value.valueType)
    {
        case SUBTREE:
            for (i = 0; i < tree->valueSize; ++i)
            {
                NvCameraJDSFreeJSON(tree->value.subtree + i);
            }
            NvOsFree(tree->value.subtree);
            tree->value.subtree = NULL;
            break;
        case CHARPTR:
            if (tree->value.chrPtr && tree->name)
            {
                NvOsFree(tree->value.chrPtr);
                tree->value.chrPtr = NULL;
            }
            break;
        case NVS8PTR:
            if (tree->value.s8Ptr && tree->name)
            {
                NvOsFree(tree->value.s8Ptr);
                tree->value.s8Ptr = NULL;
            }
            break;
        case NVU8PTR:
            if (tree->value.u8Ptr && tree->name)
            {
                NvOsFree(tree->value.u8Ptr);
                tree->value.u8Ptr = NULL;
            }
            break;
        case NVS16PTR:
            if (tree->value.s16Ptr && tree->name)
            {
                NvOsFree(tree->value.s16Ptr);
                tree->value.s16Ptr = NULL;
            }
            break;
        case NVU16PTR:
            if (tree->value.u16Ptr && tree->name)
            {
                NvOsFree(tree->value.u16Ptr);
                tree->value.u16Ptr = NULL;
            }
            break;
        case NVS32PTR:
            if (tree->value.s32Ptr && tree->name)
            {
                NvOsFree(tree->value.s32Ptr);
                tree->value.s32Ptr = NULL;
            }
            break;
        case NVU32PTR:
            if (tree->value.u32Ptr && tree->name)
            {
                NvOsFree(tree->value.u32Ptr);
                tree->value.u32Ptr = NULL;
            }
            break;
        case NVF32PTR:
            if (tree->value.f32Ptr && tree->name)
            {
                NvOsFree(tree->value.f32Ptr);
                tree->value.f32Ptr = NULL;
            }
            break;
        case NVS64PTR:
            if (tree->value.s64Ptr && tree->name)
            {
                NvOsFree(tree->value.s64Ptr);
                tree->value.s64Ptr = NULL;
            }
            break;
        case NVU64PTR:
            if (tree->value.u64Ptr && tree->name)
            {
                NvOsFree(tree->value.u64Ptr);
                tree->value.u64Ptr = NULL;
            }
            break;
        case NVF64PTR:
            if (tree->value.f64Ptr && tree->name)
            {
                NvOsFree(tree->value.f64Ptr);
                tree->value.f64Ptr = NULL;
            }
            break;
        case NVBOOLPTR:
            if (tree->value.nvBoolPtr && tree->name)
            {
                NvOsFree(tree->value.nvBoolPtr);
                tree->value.nvBoolPtr = NULL;
            }
            break;
        default:
            break;
    }
    if (tree->name)
    {
        NvOsFree(tree->name);
        tree->name = NULL;
    }

}


static JSON* NvCameraJDSGetChild(JSON *tree, const char* currentToken)
{
    NvU32 pos, pos2;
    if (!tree || !currentToken)
    {
        return NULL;
    }
    if (!(tree->value.subtree))
    {

        if (!(tree->value.subtree = NvOsAlloc(sizeof(JSON) * NVCAMERA_JDS_VALUE_VEC_SIZE)))
        {
            return NULL;
        }
        tree->valueSize = NVCAMERA_JDS_VALUE_VEC_SIZE;
        tree->value.valueType = SUBTREE;
        for (pos = 0; pos < NVCAMERA_JDS_VALUE_VEC_SIZE; ++pos)
        {
            tree->value.subtree[pos].name = NULL;
            tree->value.subtree[pos].valueSize = 0;
            tree->value.subtree[pos].value.subtree = NULL;
            tree->value.subtree[pos].value.valueType = SUBTREE;
        }
    }
    for (pos = 0; pos < tree->valueSize; ++pos)
    {
        if (tree->value.subtree[pos].name == NULL)
        {
            break;
        }
    }
    if (pos > RESIZE_THRESHOLD * tree->valueSize)
    {
        tree->valueSize *= RESIZE_FACTOR;
        if (!(tree->value.subtree =
           NvOsRealloc(tree->value.subtree, tree->valueSize * sizeof(JSON))))
        {
            return NULL;
        }
        for (pos2 = pos; pos2 < tree->valueSize; ++pos2)
        {
            tree->value.subtree[pos2].name = NULL;
            tree->value.subtree[pos2].valueSize = 0;
            tree->value.subtree[pos2].value.subtree = NULL;
            tree->value.subtree[pos2].value.valueType = SUBTREE;
        }
    }

    if (!(tree->value.subtree[pos].name = NvOsAlloc(strlen(currentToken) * sizeof(char) + 1)))
    {
        return NULL;
    }
    strcpy(tree->value.subtree[pos].name, currentToken);
    tree->value.subtree[pos].name[strlen(currentToken)] = '\0';
    return tree->value.subtree + pos;
}


static JSON* NvCameraJDSGetVertexToInsertTo(NvCameraJDS* db, char *name)
{
    char *currentToken = NULL;
    char *nameTrack;
    JSON *currentVertex = db->root;
    JSON *lastVertex = currentVertex;

    for (currentToken = strtok_r(name, ".", &nameTrack);
        currentToken != NULL; currentToken = strtok_r(NULL, ".", &nameTrack))
    {
        if (!(currentVertex = NvCameraJDSSearchVertex(currentVertex, currentToken)))
        {
            currentVertex = NvCameraJDSGetChild(lastVertex, currentToken);
        }
        lastVertex = currentVertex;
    }
    return currentVertex;
}


NvError NvCameraJDSInit(NvCameraJDS* db)
{
    const char* rootName = ".";
    db->size = NVCAMERA_JDS_MAKERNOTE_STRING_SIZE * sizeof(char);
    db->length = 0;

    if (!(db->root = NvOsAlloc(sizeof(JSON))))
    {
        return NvError_InsufficientMemory;
    }

    if (!(db->root->name = NvOsAlloc(strlen(rootName) + 1)))
    {
        return NvError_InsufficientMemory;
    }

    strcpy(db->root->name, rootName);
    db->root->name[strlen(rootName)] = '\0';
    db->root->value.valueType = SUBTREE;
    db->root->value.subtree = NULL;
    db->root->valueSize = 0;

    if(!(db->makernoteString = NvOsAlloc(NVCAMERA_JDS_MAKERNOTE_STRING_SIZE * sizeof(char))))
    {
        return NvError_InsufficientMemory;
    }

    db->makernoteString[0] = '\0';
    return NvSuccess;
}


void NvCameraJDSCleanup(NvCameraJDS* db)
{
    NvCameraJDSFreeJSON(db->root);
    NvOsFree(db->root);
    db->root = NULL;
    NvOsFree(db->makernoteString);
    db->makernoteString = NULL;
}

#define NVCAMERA_JDS_INSERT_PRIMITIVE(NVTYPE, UNION_MEMBER, VALUE_TYPE)\
NvError NvCameraJDSInsert##NVTYPE(NvCameraJDS* db, const char *name,\
    NVTYPE value)\
{\
    JSON *vertex;\
    char name_copy[NVCAMERA_JDS_MAX_NAME_LENGTH];\
    strcpy(name_copy, name);\
    name_copy[strlen(name)] = '\0';\
    vertex = NvCameraJDSGetVertexToInsertTo(db, name_copy);\
    if (!vertex)\
    {\
        return NvError_InsufficientMemory;\
    }\
    vertex->value.UNION_MEMBER = value;\
    vertex->value.valueType = VALUE_TYPE;\
    return NvSuccess;\
}

NVCAMERA_JDS_INSERT_PRIMITIVE(NvS32, s32, NVS32)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvU32, u32, NVU32)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvF32, f32, NVF32)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvS16, s16, NVS16)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvU16, u16, NVU16)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvS8, s8, NVS8)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvU8, u8, NVU8)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvS64, s64, NVS64)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvU64, u64, NVU64)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvF64, f64, NVF64)
NVCAMERA_JDS_INSERT_PRIMITIVE(NvBool, nvBool, NVBOOL)
NVCAMERA_JDS_INSERT_PRIMITIVE(char, chr, CHAR)

NvError NvCameraJDSInsertint(NvCameraJDS *db, const char *name,
    int value)
{
    return NvCameraJDSInsertNvS32(db, name, value);
}


NvError NvCameraJDSInsertfloat(NvCameraJDS *db, const char *name,
    float value)
{
    return NvCameraJDSInsertNvF32(db, name, value);
}

#undef NVCAMERA_JDS_INSERT_PRIMITIVE

#define NVCAMERA_JDS_INSERT_ARRAY(NVTYPE, UNION_MEMBER, VALUE_TYPE)\
NvError NvCameraJDSInsert##NVTYPE##Array(NvCameraJDS* db, const char *name,\
    NVTYPE* value, size_t size)\
{\
    JSON *vertex;\
    char name_copy[NVCAMERA_JDS_MAX_NAME_LENGTH];\
    strcpy(name_copy, name);\
    vertex = NvCameraJDSGetVertexToInsertTo(db, name_copy);\
    if (!vertex)\
    {\
        return NvError_InsufficientMemory;\
    }\
    vertex->value.UNION_MEMBER = NvOsAlloc(size);\
    if (!vertex->value.UNION_MEMBER)\
    {\
        return NvError_InsufficientMemory;\
    }\
    memcpy(vertex->value.UNION_MEMBER, value, size);\
    vertex->value.size = size;\
    vertex->value.valueType = VALUE_TYPE;\
    return NvSuccess;\
}\

NVCAMERA_JDS_INSERT_ARRAY(NvS32, s32Ptr, NVS32PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvU32, u32Ptr, NVU32PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvF32, f32Ptr, NVF32PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvS16, s16Ptr, NVS16PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvU16, u16Ptr, NVU16PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvS8, s8Ptr, NVS8PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvU8, u8Ptr, NVU8PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvS64, s64Ptr, NVS64PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvU64, u64Ptr, NVU64PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvF64, f64Ptr, NVF64PTR)
NVCAMERA_JDS_INSERT_ARRAY(NvBool, nvBoolPtr, NVBOOLPTR)
NVCAMERA_JDS_INSERT_ARRAY(char, chrPtr, CHARPTR)

#undef NVCAMERA_JDS_INSERT_ARRAY

/*
NvError NvCameraJDSInsertintArray(NvCameraJDS *db, const char *name,
    int* value, size_t size)
{
    return NvCameraJDSInsertNvS32Array(db, name, value, size);
}


NvError NvCameraJDSInsertfloatArray(NvCameraJDS *db, const char *name,
    float* value, size_t size)
{
    return NvCameraJDSInsertNvF32Array(db, name, value, size);
}
*/

void NvCameraJDSSerializeDB(NvCameraJDS* db)
{
    NvU32 i;
    NvCameraJDSAppendStrToMakerNote(db, "{");
    for (i = 0; i < db->root->valueSize; ++i)
    {
        if (!(db->root->value.subtree[i].name)) {
            break;
        }
        NvCameraJDSSerializeDBStatic(db, &(db->root->value.subtree[i]));
        NvCameraJDSAppendStrToMakerNote(db, ",");

    }
    db->length--;
    NvCameraJDSAppendStrToMakerNote(db, "}");
}

