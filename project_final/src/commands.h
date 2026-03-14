#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <unistd.h>
#include <stdbool.h>
#include "builtins.h"

extern int server_fd;
extern int client_fds[100];
extern int client_count;
int check_server_activity(void);

/* Reads the given file and prints it to the screen (stdout)
*/
void cat_func(char *filepath, int is_file);

/* Reads the given file and prints the word count, character count and new line count (in that order)
*/
void wc_func(char *filepath, int is_file);

/* Enters into the given path, absolute or relative.
*/
void cd_func(char *path);

void ls_func(char *str);

void list_directory(char *path, char *substring, bool recursive, int depth, int current_depth);

void start_server(char *port_number);

void close_server();

void send_message(char *port_number,char *host_name, char *message);

void start_client(char *port_number, char *host_name);

void execute_builtin(bn_ptr builtin_fn, char **token_arr);

void exec_shell_command(char **token_arr, pid_t *child_pid, size_t child_count, bool bg);

char **parse_for_pipe(char **token_arr, size_t token_count);

// ----------------- helpers -----------------------

/* Converts integer to string
*/
void int_str(int num, char *str);
void free_tokens(char **tokens);
pid_t *get_child_pids();
size_t get_child_count();

#endif

