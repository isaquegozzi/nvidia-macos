/* EXCERPT of NVIDIA open-gpu-kernel-modules src/common/sdk/nvidia/inc/nvos.h */
/* Full-file blob c061f0a2463a38ddbfc5f2ec550c138fef7f2548, 157855 B, ref main */
/* --- lines 1061-1100 --- */
// the desired size.
//
#define NVOS32_ATTR_PAGE_SIZE                                24:23
#define NVOS32_ATTR_PAGE_SIZE_DEFAULT                   0x00000000
#define NVOS32_ATTR_PAGE_SIZE_4KB                       0x00000001
#define NVOS32_ATTR_PAGE_SIZE_BIG                       0x00000002
#define NVOS32_ATTR_PAGE_SIZE_HUGE                      0x00000003

#define NVOS32_ATTR_LOCATION                                 26:25
#define NVOS32_ATTR_LOCATION_VIDMEM                     0x00000000
#define NVOS32_ATTR_LOCATION_PCI                        0x00000001
#define NVOS32_ATTR_LOCATION_ANY                        0x00000003

//
// _DEFAULT implies _CONTIGUOUS for video memory currently, but
// may be changed to imply _NONCONTIGUOUS in the future.
// _ALLOW_NONCONTIGUOUS enables falling back to the noncontiguous
// vidmem allocator if contig allocation fails.
//
#define NVOS32_ATTR_PHYSICALITY                              28:27
#define NVOS32_ATTR_PHYSICALITY_DEFAULT                 0x00000000
#define NVOS32_ATTR_PHYSICALITY_NONCONTIGUOUS           0x00000001
#define NVOS32_ATTR_PHYSICALITY_CONTIGUOUS              0x00000002
#define NVOS32_ATTR_PHYSICALITY_ALLOW_NONCONTIGUOUS     0x00000003

#define NVOS32_ATTR_COHERENCY                                31:29
#define NVOS32_ATTR_COHERENCY_UNCACHED                  0x00000000
#define NVOS32_ATTR_COHERENCY_CACHED                    0x00000001
#define NVOS32_ATTR_COHERENCY_WRITE_COMBINE             0x00000002
#define NVOS32_ATTR_COHERENCY_WRITE_THROUGH             0x00000003
#define NVOS32_ATTR_COHERENCY_WRITE_PROTECT             0x00000004
#define NVOS32_ATTR_COHERENCY_WRITE_BACK                0x00000005

// ATTR2 fields
#define NVOS32_ATTR2_NONE                               0x00000000

//
// DEFAULT          - Let lower level drivers pick optimal page kind.
// PREFER_NO_ZBC    - Prefer other types of compression over ZBC when
//                    selecting page kind.
/* --- lines 1717-1732 --- */
// and for NvRmUnmapMemory.

// Mappings can have restricted permissions (read-only, write-only).  Some
// RM implementations may choose to ignore these flags, or they may work
// only for certain memory spaces (system, video memory); in such cases,
// you may get a read/write mapping even if you asked for a read-only or
// write-only mapping.
#define NVOS33_FLAGS_ACCESS                                        1:0
#define NVOS33_FLAGS_ACCESS_READ_WRITE                             (0x00000000)
#define NVOS33_FLAGS_ACCESS_READ_ONLY                              (0x00000001)
#define NVOS33_FLAGS_ACCESS_WRITE_ONLY                             (0x00000002)

// Persistent mappings are no longer supported
#define NVOS33_FLAGS_PERSISTENT                                    4:4
#define NVOS33_FLAGS_PERSISTENT_DISABLE                            (0x00000000)
#define NVOS33_FLAGS_PERSISTENT_ENABLE                             (0x00000001)
/* --- lines 1438-1492 --- */
#define NVOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_UP           0x00000002
#define NVOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN         0x00000004
#define NVOS32_ALLOC_FLAGS_FORCE_ALIGN_HOST_PAGE        0x00000008
#define NVOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE       0x00000010
#define NVOS32_ALLOC_FLAGS_BANK_HINT                    0x00000020
#define NVOS32_ALLOC_FLAGS_BANK_FORCE                   0x00000040
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_HINT               0x00000080
#define NVOS32_ALLOC_FLAGS_ALIGNMENT_FORCE              0x00000100
#define NVOS32_ALLOC_FLAGS_BANK_GROW_UP                 0x00000000
#define NVOS32_ALLOC_FLAGS_BANK_GROW_DOWN               0x00000200
#define NVOS32_ALLOC_FLAGS_LAZY                         0x00000400
#define NVOS32_ALLOC_FLAGS_FORCE_REVERSE_ALLOC          0x00000800
#define NVOS32_ALLOC_FLAGS_NO_SCANOUT                   0x00001000
#define NVOS32_ALLOC_FLAGS_PITCH_FORCE                  0x00002000
#define NVOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED       0x00004000
#define NVOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED             0x00008000
#define NVOS32_ALLOC_FLAGS_PERSISTENT_VIDMEM            0x00010000
#define NVOS32_ALLOC_FLAGS_USE_BEGIN_END                0x00020000
#define NVOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED       0x00040000
#define NVOS32_ALLOC_FLAGS_VIRTUAL                      0x00080000
#define NVOS32_ALLOC_FLAGS_FORCE_INTERNAL_INDEX         0x00100000
#define NVOS32_ALLOC_FLAGS_ZCULL_COVG_SPECIFIED         0x00200000
#define NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED           0x00400000
#define NVOS32_ALLOC_FLAGS_FORCE_DEDICATED_PDE          0x00800000
#define NVOS32_ALLOC_FLAGS_PROTECTED                    0x01000000
#define NVOS32_ALLOC_FLAGS_KERNEL_MAPPING_MAP           0x02000000 // TODO BUG 2488679: fix alloc flag aliasing
#define NVOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE       0x02000000
#define NVOS32_ALLOC_FLAGS_SPARSE                       0x04000000
#define NVOS32_ALLOC_FLAGS_USER_READ_ONLY               0x04000000 // TODO BUG 2488682: remove this after KMD transition
#define NVOS32_ALLOC_FLAGS_DEVICE_READ_ONLY             0x08000000 // TODO BUG 2488682: remove this after KMD transition
#define NVOS32_ALLOC_FLAGS_SKIP_RESOURCE_ALLOC          0x10000000
#define NVOS32_ALLOC_FLAGS_PREFER_PTES_IN_SYSMEMORY     0x20000000
#define NVOS32_ALLOC_FLAGS_SKIP_ALIGN_PAD               0x40000000
#define NVOS32_ALLOC_FLAGS_WPR1                         0x40000000 // TODO BUG 2488672: fix alloc flag aliasing
#define NVOS32_ALLOC_FLAGS_ZCULL_DONT_ALLOCATE_SHARED_1X 0x80000000
#define NVOS32_ALLOC_FLAGS_WPR2                         0x80000000 // TODO BUG 2488672: fix alloc flag aliasing

// Internal flags used for RM's allocation paths
#define NVOS32_ALLOC_INTERNAL_FLAGS_CLIENTALLOC         0x00000001 // RM internal flags - not sure if this should be exposed even. Keeping it here.
#define NVOS32_ALLOC_INTERNAL_FLAGS_SKIP_SCRUB          0x00000004 // RM internal flags - not sure if this should be exposed even. Keeping it here.
#define NVOS32_ALLOC_FLAGS_MAXIMIZE_4GB_ADDRESS_SPACE NVOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE // Legacy name

//
// Bitmask of flags that are only valid for virtual allocations.
//
#define NVOS32_ALLOC_FLAGS_VIRTUAL_ONLY         ( \
    NVOS32_ALLOC_FLAGS_VIRTUAL                  | \
    NVOS32_ALLOC_FLAGS_LAZY                     | \
    NVOS32_ALLOC_FLAGS_EXTERNALLY_MANAGED       | \
    NVOS32_ALLOC_FLAGS_SPARSE                   | \
    NVOS32_ALLOC_FLAGS_MAXIMIZE_ADDRESS_SPACE   | \
    NVOS32_ALLOC_FLAGS_PREFER_PTES_IN_SYSMEMORY )

// COMPR_COVG_* allows for specification of what compression resources
// are required (_MIN) and necessary (_MAX).  Default behavior is for
