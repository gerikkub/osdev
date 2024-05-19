
#include "bitutils.h"

#include "edk2defines.h"

typedef void (*FreePagesFn)(EFI_PHYSICAL_ADDRESS, UINTN);
typedef void (*StallFn)(UINTN);
typedef EFI_STATUS (*AllocatePagesFn)(enum ALLOCATE_OPERATION, void*, UINTN, void*);

void GBS_FreePagesFn(EFI_PHYSICAL_ADDRESS addr, UINTN len) {
    // TODO
}

void GBS_StallFn(UINTN len) {
    // TODO
}

EFI_STATUS GBS_AllocatePagesFn(enum ALLOCATE_OPERATION op, void* dummy, UINTN len, void* todo) {
    // TODO
    return EFI_NOT_READY;
}

static EFI_GBS g_gbs = {
    .FreePages = GBS_FreePagesFn,
    .Stall = GBS_StallFn,
    .AllocatePages = GBS_AllocatePagesFn
};

EFI_GBS* gBS = NULL;
void* EfiBootServicesData = NULL;

void GBS_Setup() {
    gBS = & g_gbs;
}

void MemoryFence(void) {
    MEM_DSB();
}

void MmioWrite32(UINTN ptr, uint32_t data) {
    *(UINT32*)ptr = data;
}

void MmioWrite16(UINTN ptr, uint16_t data) {
    *(UINT16*)ptr = data;
}

void MmioWrite8(UINTN ptr, uint8_t data) {
    *(UINT8*)ptr = data;
}

uint32_t MmioRead32(UINTN ptr) {
    return *(UINT32*)ptr;
}

uint16_t MmioRead16(UINTN ptr) {
    return *(UINT16*)ptr;
}

uint8_t MmioRead8(UINTN ptr) {
    return *(UINT8*)ptr;
}


EFI_STATUS DmaMap(enum DMA_MAP_OPERATION op, UINT8* a, void* b, void* c, void* d) {
    return EFI_DEVICE_ERROR;
}

void DmaUnmap(VOID* mapping) {

}