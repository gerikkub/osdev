
#ifndef __SELECT_H__
#define __SELECT_H__

int64_t syscall_select(uint64_t select_arr_ptr, uint64_t select_len, uint64_t timeout_us, uint64_t ready_mask_ptr);

#endif