
#ifndef __QEMU_FW_CFG_H__
#define __QEMU_FW_CFG_H__

#include <stdint.h>

// https://github.com/qemu/qemu/blob/master/docs/specs/fw_cfg.txt

typedef struct __attribute__((packed)) {
    uint64_t data;
    uint16_t sel;
    uint16_t res0;
    uint32_t res1;
    uint64_t dma;
} qemu_fw_cfg_Struct;

typedef enum {
    FW_CFG_SIGNATURE = 0,
    FW_CFG_ID = 1,
    FW_CFG_FILE_DIR = 0x19,
    FW_CFG_FILE_FIRST = 0x20
} qemu_fw_cfg_sel_t;

typedef struct {		/* an individual file entry, 64 bytes total */
    uint32_t size;		/* size of referenced fw_cfg item, big-endian */
    uint16_t select;		/* selector key of fw_cfg item, big-endian */
    uint16_t reserved;
    char name[56];		/* fw_cfg item name, NUL-terminated ascii */
} qemu_fw_cfg_file_t;

typedef struct  {		/* the entire file directory fw_cfg item */
    uint32_t count;		/* number of entries, in big-endian format */
    qemu_fw_cfg_file_t f[];	/* array of file entries, see below */
} qemu_fw_cfg_files_t;

#define QEMU_FW_CFG_PHY ((volatile qemu_fw_cfg_Struct*)0x09020000)
#define QEMU_FW_CFG_VIRT ((volatile qemu_fw_cfg_Struct*)0xFFFF000009020000)

bool qemu_fw_cfg_validate(void);

uint64_t qemu_fw_cfg_read_data(qemu_fw_cfg_sel_t sel, uint8_t* data, uint64_t len);

void qemu_fw_cfg_read_dir(qemu_fw_cfg_files_t* dir_out, uint64_t num_files);

void qemu_fw_cfg_read_file(qemu_fw_cfg_file_t* file_entry, uint8_t* data, uint64_t len);


#endif