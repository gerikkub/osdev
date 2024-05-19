
#ifndef __EDK2_DEFINES_H__
#define __EDK2_DEFINES_H__

#include <stdlib.h>

#include "bitutils.h"

#define OUT
#define IN
#define STATIC static
#define CONST const
#define EFIAPI
#define OPTIONAL

#define TRUE 1
#define FALSE 0

#define DEBUG(x)
#define ASSERT(x)

#define EFI_ERROR(x) ((x) != EFI_SUCCESS)

#define BIT0 BIT(0)
#define BIT1 BIT(1)
#define BIT2 BIT(2)
#define BIT3 BIT(3)
#define BIT4 BIT(4)
#define BIT5 BIT(5)
#define BIT6 BIT(6)
#define BIT7 BIT(7)

#define BIT8 BIT(8)
#define BIT9 BIT(9)
#define BIT10 BIT(10)
#define BIT11 BIT(11)
#define BIT12 BIT(12)
#define BIT13 BIT(13)
#define BIT14 BIT(14)
#define BIT15 BIT(15)

#define BIT16 BIT(16)
#define BIT17 BIT(17)
#define BIT18 BIT(18)
#define BIT19 BIT(19)
#define BIT20 BIT(20)
#define BIT21 BIT(21)
#define BIT22 BIT(22)
#define BIT23 BIT(23)

#define BIT24 BIT(24)
#define BIT25 BIT(25)
#define BIT26 BIT(26)
#define BIT27 BIT(27)
#define BIT28 BIT(28)
#define BIT29 BIT(29)
#define BIT30 BIT(30)
#define BIT31 BIT(30)

#define EFI_SIZE_TO_PAGES(x) (((x) + 4095) / 4096)


typedef void VOID;
typedef int EFI_STATUS;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef int8_t CHAR8;
typedef uint8_t UINT8;
typedef uint64_t UINTN;
typedef uint8_t BOOLEAN;

// TODO: 32 or 64 bits?
typedef uint64_t EFI_PHYSICAL_ADDRESS;

typedef void* EFI_HANDLE;
typedef int EFI_LOCK;
typedef void* EFI_EVENT;
typedef void* EFI_SIMPLE_NETWORK_PROTOCOL;
typedef void* EFI_ADAPTER_INFORMATION_PROTOCOL;

typedef int RETURN_STATUS;

enum {
    EFI_SUCCESS = 0,
    EFI_NOT_READY = 1,
    EFI_NOT_FOUND = 2,
    EFI_TIMEOUT = 3,
    EFI_DEVICE_ERROR = 4,
};

typedef struct {
    UINT8 Addr[32];
} EFI_MAC_ADDRESS;

typedef struct {
    EFI_MAC_ADDRESS CurrentAddress;
} EFI_SIMPLE_NETWORK_MODE;

typedef struct {
    UINT32 Data1;
    UINT16 Data2;
    UINT16 Data3;
    UINT8 Data4[8];
} GUID;

typedef GUID EFI_GUID;

enum ALLOCATE_OPERATION {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress
};

typedef void (*FreePagesFn)(EFI_PHYSICAL_ADDRESS, UINTN);
typedef void (*StallFn)(UINTN);
typedef EFI_STATUS (*AllocatePagesFn)(enum ALLOCATE_OPERATION, void*, UINTN, void*);

typedef struct EFI_GBS {
    FreePagesFn FreePages;
    StallFn Stall;
    AllocatePagesFn AllocatePages;
} EFI_GBS;

extern EFI_GBS* gBS;
extern void* EfiBootServicesData;

void MemoryFence(void);
void MmioWrite32(UINTN ptr, uint32_t data);
void MmioWrite16(UINTN ptr, uint16_t data);
void MmioWrite8(UINTN ptr, uint8_t data);

uint32_t MmioRead32(UINTN ptr);
uint16_t MmioRead16(UINTN ptr);
uint8_t MmioRead8(UINTN ptr);

enum DMA_MAP_OPERATION {
    MapOperationBusMasterRead,
    MapOperationBusMasterWrite,
    MapOperationBusMasterCommonBuffer,
    MapOperationMaximum
};

EFI_STATUS DmaMap(enum DMA_MAP_OPERATION, UINT8*, void*, void*, void*);
void DmaUnmap(VOID* mapping);

#include "PcdLib.h"

#endif