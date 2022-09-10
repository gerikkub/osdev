
#include <stdint.h>

#include <kernel/assert.h>
#include <kernel/fd.h>
#include <kernel/lib/llist.h>
#include <kernel/lib/vmalloc.h>
#include <kernel/fs/file.h>

#include <include/k_ioctl_common.h>

#include <stdlib/string.h>
#include <stdlib/bitutils.h>

void* file_create_ctx(file_ctx_t* file_ctx) {

    file_ctx_t* ctx_out = vmalloc(sizeof(file_ctx_t));

    *ctx_out = *file_ctx;

    return ctx_out;
}

int64_t file_read_op(void* ctx, uint8_t* buffer, const int64_t size, const uint64_t flags) {

    file_ctx_t* file_ctx = ctx;

    uint64_t remaining = size;
    uint64_t count = 0;
    uint64_t data_idx = 0;
    file_data_entry_t* entry;

    FOR_LLIST(file_ctx->file_data, entry)
        if (remaining == 0) {
            break;
        }

        uint64_t end_idx = data_idx + entry->len;
        if (file_ctx->seek_idx >= data_idx &&
            file_ctx->seek_idx < end_idx) {
            
            if (!entry->available) {
                ASSERT(file_ctx->populate_op != NULL);
                file_ctx->populate_op(file_ctx->op_ctx, entry);
                ASSERT(entry->available);
            }

            uint64_t remaining_block = end_idx - file_ctx->seek_idx;
            uint64_t entry_idx = file_ctx->seek_idx - data_idx;
            uint64_t copy_len = remaining < remaining_block ? remaining : remaining_block;

            memcpy(&buffer[count], &entry->data[entry_idx], copy_len);
            remaining -= copy_len;
            count += copy_len;
            file_ctx->seek_idx += copy_len;
        }

        data_idx += entry->len;
    END_FOR_LLIST()

    return count;
}

int64_t file_write_op(void* ctx, const uint8_t* buffer, const int64_t size, const uint64_t flags) {

    file_ctx_t* file_ctx = ctx;

    if (file_ctx->can_write) {
        
        uint64_t remaining = size;
        uint64_t count = 0;
        uint64_t data_idx = 0;
        file_data_entry_t* entry;

        FOR_LLIST(file_ctx->file_data, entry)
            if (remaining == 0) {
                break;
            }

            uint64_t end_idx = data_idx + entry->len;
            if (file_ctx->seek_idx >= data_idx &&
                file_ctx->seek_idx < end_idx) {

                if (!entry->available) {
                    ASSERT(file_ctx->populate_op != NULL);
                    file_ctx->populate_op(file_ctx->op_ctx, entry);
                    ASSERT(entry->available);
                }

                uint64_t remaining_block = end_idx - file_ctx->seek_idx;
                uint64_t entry_idx = file_ctx->seek_idx - data_idx;
                uint64_t copy_len = remaining < remaining_block ? remaining : remaining_block;

                memcpy(&entry->data[entry_idx], &buffer[count], copy_len);
                remaining -= copy_len;
                count += copy_len;
                entry->dirty = 1;
                file_ctx->seek_idx += copy_len;
            }

            data_idx += entry->len;
        END_FOR_LLIST()

        return count;

    } else {
        return -1;
    }
}

int64_t file_ioctl_op(void* ctx, const uint64_t ioctl, const uint64_t* args, const uint64_t arg_count) {

    file_ctx_t* file_ctx = ctx;
    switch (ioctl) {
        case IOCTL_SEEK:
            if (arg_count != 1) {
                return -1;
            }

            uint64_t arg_seek = args[0];

            if (arg_seek > file_ctx->size) {
                return -1;
            } else {
                file_ctx->seek_idx = arg_seek;
                return 0;
            }
            break;
        case BLK_IOCTL_SIZE:
            return file_ctx->size;
            break;
        default:
            return -1;
    }

    return -1;
}

int64_t file_close_op(void* ctx) {

    file_ctx_t* file_ctx = ctx;
    file_data_entry_t* entry;
    if (file_ctx->can_write) {
        ASSERT(file_ctx->flush_data_op != NULL);
        FOR_LLIST(file_ctx->file_data, entry)
            if (entry->dirty) {
                file_ctx->flush_data_op(file_ctx->op_ctx, entry);
            }
        END_FOR_LLIST()
    }

    file_ctx->close_op(file_ctx->op_ctx);

    FOR_LLIST(file_ctx->file_data, entry)
        vfree(entry);
    END_FOR_LLIST()
    llist_free_all(file_ctx->file_data);
    vfree(file_ctx);

    return 0;
}