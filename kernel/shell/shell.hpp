#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void shell_run(int start_y);
void serial_shell_init();
void serial_shell_poll(char c);
void shell_update_cursor();

#ifdef __cplusplus
}
#endif
