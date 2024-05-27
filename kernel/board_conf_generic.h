
#ifndef __BOARD_CONF_GENERIC_H__
#define __BOARD_CONF_GENERIC_H__

void board_init_main_early(void);
void board_init_mappings(void);
void board_init_early_console(void);
void board_init_devices(void);
void board_discover_devices(void);

void board_loop(void);

#endif