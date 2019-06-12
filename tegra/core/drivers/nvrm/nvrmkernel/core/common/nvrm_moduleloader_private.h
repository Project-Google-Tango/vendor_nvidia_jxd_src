/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_MODULELOADER_PRIVATE_H
#define INCLUDED_NVRM_MODULELOADER_PRIVATE_H

#include "nvrm_moduleloader.h"
#include "nvrm_memmgr.h"
/// Data types as defined by the ELF specification.
typedef unsigned int        Elf32_Addr;   ///< Unsigned program address
typedef unsigned short int  Elf32_Half;   ///< Unsigned medium integer
typedef unsigned int        Elf32_Off;    ///< Unsigned file offset
typedef signed int          Elf32_Sword;  ///< Signed large integer
typedef unsigned int        Elf32_Word;   ///< Unsigned large integer
typedef unsigned char       Elf32_Byte;  ///< Unsigned byte

#define BIT(x) (1 << x)

/// Macros for accessing the fields of r_info i.e. relocation info
#define ELF32_R_SYM(rinfo)  ((rinfo) >> 8)           ///< Get symbol index
#define ELF32_R_TYPE(rinfo) ((unsigned char)(rinfo)) ///< Get relocation code

#define LOAD_ADDRESS        0x11001000
#define IRAM_PREF_EXT_ADDRESS   0x50000000
#define IRAM_MAND_ADDRESS   0x40000000
#define DRAM_MAND_ADDRESS   0x10000000
#define DT_ARM_SYMTABSZ     0x70000001
#define DT_ARM_RESERVED1    0x70000000
/// ELF program segment permissions
enum
{
    /// Execute only
    PF_X = 0x1,

    /// Write only
    PF_W,

    /// Write, execute
    PF_XW,

    /// Read only
    PF_R,

    /// Read, execute
    PF_XR,

    /// Read, write
    PF_RW,

    /// Read, write, execute
    PF_XRW
};

/// Architecture type
enum
{
    EM_ARM = 0x28
};
/// ELF magic number
enum
{
    ELF_MAG0 = 0x7F
};
/// ELF file header format.
typedef struct
{
    Elf32_Byte e_ident[4];              ///< Magic number
    Elf32_Byte EI_CLASS;                ///< File class
    Elf32_Byte EI_DATA;                 ///< Data encoding
    Elf32_Byte EI_VERSION;              ///< File version
    Elf32_Byte e_pad[16-7];             ///< Padding. Total 16 bytes.
    Elf32_Half e_type;                  ///< Object file type 
    Elf32_Half e_machine;               ///< Architecture 
    Elf32_Word e_version;               ///< Object file version 
    Elf32_Addr e_entry;                 ///< Entry point virtual address 
    Elf32_Off e_phoff;                  ///< Program header table file offset 
    Elf32_Off e_shoff;                  ///< Section header table file offset 
    Elf32_Word e_flags;                 ///< Processor-specific flags 
    Elf32_Half e_ehsize;                ///< ELF header size in bytes 
    Elf32_Half e_phentsize;             ///< Program header table entry size 
    Elf32_Half e_phnum;                 ///< Program header table entry count 
    Elf32_Half e_shentsize;             ///< Section header table entry size 
    Elf32_Half e_shnum;                 ///< Section header table entry count 
    Elf32_Half e_shstrndx;              ///< Section header string table index 
} Elf32_Ehdr;


/// ELF program header entry format.
typedef struct
{
    Elf32_Word p_type;                  ///< Segment type 
    Elf32_Off p_offset;                 ///< Segment file offset 
    Elf32_Addr p_vaddr;                 ///< Segment virtual address 
    Elf32_Addr p_paddr;                 ///< Segment physical address 
    Elf32_Word p_filesz;                ///< Segment size in file 
    Elf32_Word p_memsz;                 ///< Segment size in memory 
    Elf32_Word p_flags;                 ///< Segment flags 
    Elf32_Word p_align;                 ///< Segment alignment     
} Elf32_Phdr;

/// ELF program segment types
enum 
{
    /// The array element is unused; other members' values are undefined.
    /// This type lets the program header table have ignored entries.
    PT_NULL, 

    /// The array element specifies a loadable segment,
    /// described by p_filesz and p_memsz.
    PT_LOAD,

    /// The array element specifies dynamic linking information.
    PT_DYNAMIC,
        
    /// The array element specifies the location and size of a null-terminated
    /// path name to invoke as an interpreter.
    PT_INTERP,
        
    /// The array element specifies the location and
    /// size of auxiliary information.
    PT_NOTE,
        
    /// This segment type is reserved but has unspecified semantics.
    /// Programs that contain an array element of this type
    /// do not conform to the ABI.
    PT_SHLIB,
        
    /// The array element, if present, specifies the location and size of the
    /// program header table itself, both in the file and
    /// in the memory image of the program.
    PT_PHDR,
        
    /// Low end of the inclusive range that is reserved for processor-specific
    /// semantics.
    PT_LOPROC = 0x70000000,
        
    /// High end of the inclusive range that is reserved for processor-specific
    /// semantics.
    PT_HIPROC = 0x7fffffff
};

/// ELF section header entry format.
typedef struct
{
    Elf32_Word sh_name;             ///< Section name (string table index) 
    Elf32_Word sh_type;             ///< Section type 
    Elf32_Word sh_flags;            ///< Section flags 
    Elf32_Addr sh_addr;             ///< Section virtual addr at execution 
    Elf32_Off sh_offset;            ///< Section file offset 
    Elf32_Word sh_size;             ///< Section size in bytes 
    Elf32_Word sh_link;             ///< Link to another section 
    Elf32_Word sh_info;             ///< Additional section information 
    Elf32_Word sh_addralign;        ///< Section alignment 
    Elf32_Word sh_entsize;          ///< Entry size if section holds table 
} Elf32_Shdr;

/// ELF section header entry types.
enum
{
    SHT_NULL = 0,               ///< Section header table entry unused
    SHT_PROGBITS,               ///< Program data 
    SHT_SYMTAB,                 ///< Symbol table 
    SHT_STRTAB,                 ///< String table 
    SHT_RELA,                   ///< Relocation entries with addends 
    SHT_HASH,                   ///< Symbol hash table 
    SHT_DYNAMIC,                ///< Dynamic linking information 
    SHT_NOTE,                   ///< Notes 
    SHT_NOBITS,                 ///< Program space with no data (bss) 
    SHT_REL,                    ///< Relocation entries, no addends 
    SHT_SHLIB,                  ///< Reserved 
    SHT_DYNSYM,                 ///< Dynamic linker symbol table 
    SHT_INIT_ARRAY,             ///< Code initialization array
    SHT_FINI_ARRAY,             ///< Code finalization array
    SHT_PREINIT_ARRAY,          ///< Code pre-inialization array
    SHT_GROUP,                  ///< Group
    SHT_SYMTAB_SHNDX,           ///< Symbol table index
};
#define SHT_LOPROC 0x70000000    ///< Start of processor-specific 
#define SHT_HIPROC 0x7fffffff    ///< End of processor-specific 
#define SHT_LOUSER 0x80000000    ///< Start of application-specific 
#define SHT_HIUSER 0xffffffff     ///< End of application-specific 

/// ELF section header entry flags.
enum
{
    SHF_WRITE       = BIT(0),
    SHF_ALLOC       = BIT(1),
    SHF_EXECINSTR   = BIT(2),
    SHF_MERGE       = BIT(4),
    SHF_STRINGS     = BIT(5),
    SHF_INFO_LINKS  = BIT(6),
    SHF_LINK_ORDER  = BIT(7),
    SHF_OS_NONCONFORMING = BIT(8),
    SHF_GROUP       = BIT(9),
    SHF_TLS         = BIT(10),
    SHF_ENTRYSECT   = BIT(28),
    SHF_COMDEF      = BIT(31)
};

/// ELF dynamic section type flags
enum 
{
    DT_NULL             = 0,            ///< Marks end of dynamic section 
    DT_NEEDED           = 1,            ///< Name of needed library 
    DT_PLTRELSZ         = 2,            ///< Size in bytes of PLT relocs 
    DT_PLTGOT           = 3,            ///< Processor defined value 
    DT_HASH             = 4,            ///< Address of symbol hash table 
    DT_STRTAB           = 5,            ///< Address of string table 
    DT_SYMTAB           = 6,            ///< Address of symbol table 
    DT_RELA             = 7,            ///< Address of Rela relocs 
    DT_RELASZ           = 8,            ///< Total size of Rela relocs 
    DT_RELAENT          = 9,            ///< Size of one Rela reloc 
    DT_STRSZ            = 10,           ///< Size of string table 
    DT_SYMENT           = 11,           ///< Size of one symbol table entry 
    DT_INIT             = 12,           ///< Address of init function 
    DT_FINI             = 13,           ///< Address of termination function 
    DT_SONAME           = 14,           ///< Name of shared object 
    DT_RPATH            = 15,           ///< Library search path (deprecated) 
    DT_SYMBOLIC         = 16,           ///< Start symbol search here 
    DT_REL              = 17,           ///< Address of Rel relocs 
    DT_RELSZ            = 18,           ///< Total size of Rel relocs 
    DT_RELENT           = 19,           ///< Size of one Rel reloc 
    DT_PLTREL           = 20,           ///< Type of reloc in PLT 
    DT_DEBUG            = 21,           ///< For debugging; unspecified 
    DT_TEXTREL          = 22,           ///< Reloc might modify .text 
    DT_JMPREL           = 23,           ///< Address of PLT relocs 
    DT_BIND_NOW         = 24,           ///< Process relocations of object 
    DT_INIT_ARRAY       = 25,           ///< Array with addresses of init fct 
    DT_FINI_ARRAY       = 26,           ///< Array with addresses of fini fct 
    DT_INIT_ARRAYSZ     = 27,           ///< Size in bytes of DT_INIT_ARRAY 
    DT_FINI_ARRAYSZ     = 28,           ///< Size in bytes of DT_FINI_ARRAY 
    DT_RUNPATH          = 29,           ///< Library search path 
    DT_FLAGS            = 30,           ///< Flags for the object being loaded 
    DT_ENCODING         = 32,           ///< Start of encoded range 
    DT_PREINIT_ARRAY    = 32,           ///< Array with addresses of preinit fct
    DT_PREINIT_ARRAYS   = 33,           ///< size in bytes of DT_PREINIT_ARRAY 
    DT_NUM              = 34,           ///< Number used 
    DT_LOOS             = 0x6000000d,   ///< Start of OS-specific 
    DT_HIOS             = 0x6ffff000,   ///< End of OS-specific 
    DT_LOPROC           = 0x70000000,   ///< Start of processor-specific 
    DT_HIPROC           = 0x7fffffff    ///< End of processor-specific 
};

/// The dynamic segment starts with this "dynamic section".
/// Array of types and values that defines attributes of the dynamic segment.
struct Elf32_Dyn
{
    Elf32_Sword d_tag;      ///< Dynamic segment attribute type.
    Elf32_Word d_val;       ///< Dynamic segment attribute value.
} __attribute__((packed));
typedef struct Elf32_Dyn Elf32_Dyn;

/// ARM Relocation table entries with implicit addends.
struct Elf32_Rel
{        
    /// This member gives the location at which to apply the relocation action. 
    /// For a relocatable file, the value is the byte offset from the beginning 
    /// of the section to the storage unit affected by the relocation. For an
    /// executable file or a shared object, the value is the virtual address
    /// of the storage unit affected by the relocation.
    Elf32_Addr r_offset;    
        
    /// This member gives both the symbol table index with respect to which the
    /// relocation must be made, and the type of relocation to apply.
    /// For example, a call instruction's relocation entry would hold the 
    /// symbol table index of the function being called.
    /// If the index is STN_UNDEF, the undefined symbol index,
    /// the relocation uses 0 as the symbol value. Relocation types are
    /// processor-specific; descriptions of their behavior appear in 
    /// section 4.5, Relocation types. When the text in section 4.5
    /// refers to a relocation entry's relocation type or symbol table index, it
    /// means the result of applying ELF32_R_TYPE or ELF32_R_SYM, respectively,
    /// to the entry's r_info member.
    /// #define ELF32_R_SYM(i) ((i)>>8)
    /// #define ELF32_R_TYPE(i) ((unsigned char)(i))
    /// #define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))
    Elf32_Word r_info;      
} __attribute((packed));
typedef struct Elf32_Rel Elf32_Rel;
    
/// ARM Relocation table entries with explicit addends.
typedef struct 
{        
    Elf32_Addr r_offset;    
    Elf32_Word  r_info;    
    /// This member gives the addend or constant that needs to be added.
    Elf32_Sword r_addend;  
} Elf32_Rela;

/// ELF symbol entry format
typedef struct
{
    Elf32_Word st_name;        ///< Symbol name (string tbl index) 
    Elf32_Addr st_value;       ///< Symbol value 
    Elf32_Word st_size;        ///< Symbol size 
    Elf32_Byte st_info;        ///< Symbol type and binding. 
    Elf32_Byte st_other;       ///< Symbol visibility 
    Elf32_Half st_shndx;       ///< Section index 
} Elf32_Sym;

/// ELF symbol bindings for top nibble of Elf32_Sym.st_info.
enum 
{
    /// Local symbols are not visible outside the object file
    /// containing their definition. Local symbols of the same name may exist 
    /// in multiple files without interfering with each other.
    STB_LOCAL = 0,

    /// Global symbols are visible to all object files being combined.
    /// One file’s definition of a global symbol will satisfy another file’s
    /// undefined reference to the same global symbol.
    STB_GLOBAL = 1,
    
    /// Weak symbols resemble global symbols, but their definitions have lower precedence.
    STB_WEAK = 2,
    
    /// Values in this inclusive range are reserved for processor-specific semantics.
    STB_LOPROC = 13,
    STB_HIPROC = 15
};


/// ELF symbol types for bottom nibble of Elf32_Sym.st_info.
enum 
{
    /// The symbol’s type is not specified.
    STT_NOTYPE = 0,

    /// The symbol is associated with a data object,
    /// such as a variable, an array, etc.
    STT_OBJECT = 1, 

    /// The symbol is associated with a function or other executable code.
    STT_FUNC = 2, 

    /// The symbol is associated with a section. Symbol table entries of 
    /// this type exist primarily for relocation and normally
    /// have STB_LOCAL binding.
    STT_SECTION = 3,

    /// Conventionally, the symbol’s name gives the name of the source file
    /// associated with the object file. A file symbol has STB_LOCAL binding,
    /// its section index is SHN_ABS, and it precedes the other STB_LOCAL symbols
    /// for the file, if it is present.
    STT_FILE = 4, 
    
    /// Values in this inclusive range are reserved for
    /// processor-specific semantics.
    STT_LOPROC = 13,
    STT_HIPROC = 15
};

/// ELF "special" index values for Elf32_Sym.st_shndx field.
enum 
{
    /// This section table index means the symbol is undefined. 
    /// When the link editor combines this object file with
    /// another that defines the indicated symbol, this file’s
    /// references to the symbol will be linked to the actual definition.
    SHN_UNDEF = 0,

    /// The symbol has an absolute value that will not change because of relocation.
    SHN_ABS = 0xfff1,

    /// The symbol labels a common block that has not yet been allocated. 
    /// The symbol’s value gives alignment constraints, similar to a section’s
    /// sh_addralign member. That is, the link editor will allocate the storage
    /// for the symbol at an address that is a multiple of st_value.
    /// The symbol’s size tells how many bytes are required.
    SHN_COMMON = 0xfff2
};

/// ARM specific relocation codes
enum 
{
    R_ARM_NONE = 0,
    R_ARM_PC24,
    R_ARM_ABS32,
    R_ARM_REL32,
    R_ARM_PC13,
    R_ARM_ABS16,
    R_ARM_ABS12,
    R_ARM_THM_ABS5,
    R_ARM_ABS8,
    R_ARM_SBREL32,
    R_ARM_THM_PC22,
    R_ARM_THM_PC8,
    R_ARM_AMP_VCALL9,
    R_ARM_SWI24,
    R_ARM_THM_SWI8,
    R_ARM_XPC25,
    R_ARM_THM_XPC22,
    R_ARM_TLS_DTPMOD32,
    R_ARM_TLS_DTPOFF32,
    R_ARM_TLS_TPOFF32,
    R_ARM_COPY,
    R_ARM_GLOB_DAT,
    R_ARM_JUMP_SLOT,
    R_ARM_RELATIVE,
    R_ARM_GOTOFF32,
    R_ARM_BASE_PREL,
    R_ARM_GOT_BREL,
    R_ARM_PLT32,
    R_ARM_CALL,
    R_ARM_JUMP24,
    R_ARM_THM_JUMP24,
    R_ARM_BASE_ABS,
    R_ARM_ALU_PCREL_7_0,
    R_ARM_ALU_PCREL_15_8,
    R_ARM_ALU_PCREL_23_15,
    R_ARM_LDR_SBREL_11_0_NC,
    R_ARM_ALU_SBREL_19_12_NC,
    R_ARM_ALU_SBREL_27_20_CK,
    R_ARM_TARGET1,
    R_ARM_SBREL31,
    R_ARM_V4BX,
    R_ARM_TARGET2,
    R_ARM_PREL31,
    R_ARM_MOVW_ABS_NC,
    R_ARM_MOVT_ABS,
    R_ARM_MOVW_PREL_NC,
    R_ARM_MOVT_PREL,
    R_ARM_THM_MOVW_ABS_NC,
    R_ARM_THM_MOVT_ABS,
    R_ARM_THM_MOVW_PREL_NC,
    R_ARM_THM_MOVT_PREL,
    R_ARM_THM_JUMP19,
    R_ARM_THM_JUMP6,
    R_ARM_THM_ALU_PREL_11_0,
    R_ARM_THM_PC12,
    R_ARM_ABS32_NOI,
    R_ARM_REL32_NOI,
    R_ARM_ALU_PC_G0_NC,
    R_ARM_ALU_PC_G0,
    R_ARM_ALU_PC_G1_NC,
    R_ARM_ALU_PC_G1,
    R_ARM_ALU_PC_G2,
    R_ARM_LDR_PC_G1,
    R_ARM_LDR_PC_G2,
    R_ARM_LDRS_PC_G0,
    R_ARM_LDRS_PC_G1,
    R_ARM_LDRS_PC_G2,
    R_ARM_LDC_PC_G0,
    R_ARM_LDC_PC_G1,
    R_ARM_LDC_PC_G2,
    R_ARM_ALU_SB_G0_NC,
    R_ARM_ALU_SB_G0,
    R_ARM_ALU_SB_G1_NC,
    R_ARM_ALU_SB_G1,
    R_ARM_ALU_SB_G2,
    R_ARM_LDR_SB_G0,
    R_ARM_LDR_SB_G1,
    R_ARM_LDR_SB_G2,
    R_ARM_LDRS_SB_G0,
    R_ARM_LDRS_SB_G1,
    R_ARM_LDRS_SB_G2,
    R_ARM_LDC_SB_G0,
    R_ARM_LDC_SB_G1,
    R_ARM_LDC_SB_G2,
    R_ARM_MOVW_BREL_NC,
    R_ARM_MOVT_BREL,
    R_ARM_MOVW_BREL,
    R_ARM_THM_MOVW_BREL_NC,
    R_ARM_THM_MOVT_BREL,
    R_ARM_THM_MOVW_BREL,
    R_ARM_TLS_GOTDESC,
    R_ARM_TLS_CALL,
    R_ARM_TLS_DESCSEQ,
    R_ARM_THM_TLS_CALL,
    R_ARM_PLT32_ABS,
    R_ARM_GOT_ABS,
    R_ARM_GOT_PREL,
    R_ARM_GOT_BREL12,
    R_ARM_GOTOFF12,
    R_ARM_GOTRELAX,
    R_ARM_GNU_VTENTRY,
    R_ARM_GNU_VTINHERIT,
    R_ARM_THM_JUMP11,
    R_ARM_THM_JUMP8,
    R_ARM_TLS_GD32,
    R_ARM_TLS_LDM32,
    R_ARM_TLS_LDO32,
    R_ARM_TLS_IE32,
    R_ARM_TLS_LE32,
    R_ARM_TLS_LDO12,
    R_ARM_TLS_LE12,
    R_ARM_TLS_IE12GP,
    R_ARM_ME_TOO = 128,
    R_ARM_THM_TLS_DESCSEQ16,
    R_ARM_THM_TLS_DESCSEQ32,
    R_ARM_RXPC25 = 249,
    R_ARM_RSBREL32,
    R_ARM_THM_RPC22,
    R_ARM_RREL32,
    R_ARM_RABS32,
    R_ARM_RPC24,
    R_ARM_RBASE
};

/// A linked list of load segment records
typedef struct SegmentRec SegmentNode;

struct SegmentRec
{
    NvRmMemHandle pLoadRegion;
    NvU32 LoadAddress; 
    NvU32 Index;
    NvU32 VirtualAddr; 
    NvU32 MemorySize; 
    NvU32 FileOffset; 
    NvU32 FileSize;
    void* MapAddr;
    SegmentNode *Next;
};

/// ModuleLoader handle structure
typedef struct NvRmLibraryRec 
{
    void* pLibBaseAddress;
    void* EntryAddress;
    SegmentNode *pList;
} NvRmLibHandle;

NvError
NvRmPrivLoadKernelLibrary(NvRmDeviceHandle hDevice,
                      const char *pLibName,
                      NvRmLibraryHandle *hLibHandle);

/// Add a load region to the segment list
SegmentNode* AddToSegmentList(SegmentNode *pList,
                      NvRmMemHandle pRegion,
                      Elf32_Phdr Phdr,
                      NvU32 Idx,
                      NvU32 PhysAddr,
                      void* MapAddr);

/// Apply the relocation code based on relocation info from relocation table 
NvError
ApplyRelocation(SegmentNode *pList,
                NvU32 FileOffset,
                NvU32 SegmentOffset,
                NvRmMemHandle pRegion,
                const Elf32_Rel *pRel);

/// Get the special section name for a given section type and flag
NvError
GetSpecialSectionName(Elf32_Word SectionType,
                      Elf32_Word SectionFlags,
                      const char** SpecialSectionName);

/// Parse the dynamic segment of ELF to extract the relocation table
 NvError
ParseDynamicSegment(SegmentNode *pList,
                    const char* pSegmentData,
                    size_t SegmentSize,
                    NvU32 DynamicSegmentOffset);

/// Parse ELF library and load the relocated library segments for a given library name
NvError NvRmPrivLoadLibrary(NvRmDeviceHandle hDevice,
                                                            const char *Filename,
                                                            NvU32 Address,
                                                            NvBool IsApproachGreedy,
                                                            NvRmLibraryHandle *hLibHandle);

/// Get the symbol address. In phase1, this api will return the entry point address of the module
NvError
NvRmPrivGetProcAddress(NvRmLibraryHandle Handle,
               const char *pSymbol,
               void **pSymAddress);
/// Free the ELF library by unloading the library from memory
NvError NvRmPrivFreeLibrary(NvRmLibHandle *hLibHandle);

/// Unmap memory segments
void UnMapRegion(SegmentNode *pList); 
/// Unload segments
void RemoveRegion(SegmentNode *pList);

void parseElfHeader(Elf32_Ehdr *elf);

NvError 
LoadLoadableProgramSegment(NvOsFileHandle elfSourceHandle,
            NvRmDeviceHandle hDevice,
            NvRmLibraryHandle hLibHandle,
            Elf32_Phdr Phdr,
            Elf32_Ehdr Ehdr,
            const NvRmHeap * Heaps,
            NvU32 NumHeaps,
            NvU32 loop,
            const char *Filename,
            SegmentNode **segmentList);

NvError 
parseProgramSegmentHeaders(NvOsFileHandle elfSourceHandle, 
            NvU32 segmentHeaderOffset, 
            NvU32 segmentCount);

 NvError
parseSectionHeaders(NvOsFileHandle elfSourceHandle, 
            Elf32_Ehdr *elf);

NvError
loadSegmentsInFixedMemory(NvOsFileHandle elfSourceHandle, 
                        Elf32_Ehdr *elf, NvU32 segmentIndex, void **loadaddress);
#endif
