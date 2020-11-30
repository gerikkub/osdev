
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "drivers/qemu_fw_cfg.h"

#include "stdlib/bitutils.h"

bool qemu_fw_cfg_validate(void) {
    uint8_t buf[4];
    qemu_fw_cfg_read_data(FW_CFG_SIGNATURE, buf, sizeof(buf));

    return memcmp("QEMU", buf, 4) == 0;
}

uint64_t qemu_fw_cfg_read_data(qemu_fw_cfg_sel_t sel, uint8_t* data, uint64_t len) {
    QEMU_FW_CFG_VIRT->sel = sel;

    MEM_DMB();
/*
    while (len > 8) {
        *data = QEMU_FW_CFG_VIRT->data;
        len -= 8;
        data++;
    }
    */

    uint8_t *cfg_8 = (uint8_t *)&QEMU_FW_CFG_VIRT->data;
    while (len > 0) {
        *data = *cfg_8;
        data++;
        len--;
    }

    return len;
}

void qemu_fw_cfg_read_dir(qemu_fw_cfg_files_t* dir_out, uint64_t num_files) {
    uint64_t len = ((num_files * sizeof(qemu_fw_cfg_file_t)) + sizeof(qemu_fw_cfg_files_t));
    qemu_fw_cfg_read_data(FW_CFG_FILE_DIR, (uint8_t*)dir_out, len);

    dir_out->count = en_swap_32(dir_out->count);

    for (uint64_t idx = 0; idx < dir_out->count; idx++) {
        dir_out->f[idx].size = en_swap_32(dir_out->f[idx].size);
        dir_out->f[idx].select = en_swap_16(dir_out->f[idx].select);
    }
} 

void qemu_fw_cfg_read_file(qemu_fw_cfg_file_t* file_entry, uint8_t* data, uint64_t len) {
    qemu_fw_cfg_read_data(file_entry->select, data, len);
}

