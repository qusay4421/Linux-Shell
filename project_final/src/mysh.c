#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>
#include "commands.h"
#include "variables.h"
#include "builtins.h"
#include "io_helpers.h"

#define MAX_CHILD_PROCESSES 100

bool bpipe;
bool bg;
int universal_child_count = 0;
char *cmd_arr[MAX_CHILD_PROCESSES] = {NULL};
pid_t child_pids[MAX_CHILD_PROCESSES] = {-1};
size_t child_count = 0;

void reap_zombies() {
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { // if child returns
        // Remove the terminated child PID from the list
        for (size_t i = 0; i < child_count; i++) {
            if (child_pids[i] == pid) {
                display_message("[");
                char buffer[MAX_CHILD_PROCESSES];
                int_str(i + 1, buffer);
                display_message(buffer);
                display_message("]+  Done ");
                display_message(cmd_arr[i]);
                display_message("\n");
                child_pids[i] = -1; // Mark as terminated
                if (cmd_arr[i] != NULL) { free(cmd_arr[i]); }
                cmd_arr[i] = NULL;  // Mark as unused
                break;
            }
        }
    }
    // Compact the child_pids array by removing terminated PIDs
    size_t j = 0;
    for (size_t i = 0; i < child_count; i++) {
        if (child_pids[i] != -1) {
            child_pids[j] = child_pids[i];
            cmd_arr[j] = cmd_arr[i]; // Move the command to the new position
            j++;
        }
    }
    universal_child_count = child_count;
    child_count = j;
    if (child_count == 0) {
        universal_child_count = 0;
    }
}

void stop_all_child_processes(pid_t *cp, size_t cc) {
    for (size_t i = 0; i < cc; i++) {
        if (cp[i] > 0) {
            // printf("Stopping child process (PID: %d)\n", cp[i]);
            kill(cp[i], SIGTERM); // Send SIGTERM to the child process
        }
    }
    // Reap all terminated child processes
    reap_zombies();
    // Clear the child_pids array
    cc = 0;
}

void handle_ls(char **token_arr, size_t token_count) {
    char *str = malloc(MAX_STR_LEN + 1);
    if (!str) {
        display_error("ERROR: Failed to allocate memory for str.", "");
        exit(1);
    }
    str[0] = '\0';
    for (size_t i = 1; i < token_count; i++) {
        strncat(str, token_arr[i], strlen(token_arr[i]));
        // dont add space at the end
        if (i != token_count - 1) {
            strncat(str, " ", 2);
        }
    }
    ls_func(str);
    free(str);
}

void handle_single_arg(char **token_arr) {
    if (strcmp(token_arr[0], "cat") == 0) {
        cat_func(NULL, 0);
        return;
    }
    if (strcmp(token_arr[0], "wc") == 0) {
        wc_func(NULL, 0);
        return;
    }
    if (strcmp(token_arr[0], "ps") == 0) {
        for (size_t i = 0; i < child_count; i++) {
            // only first part of command
            char *first_part = strtok(cmd_arr[i], " ");
            display_message(first_part);
            display_message(" ");
            char buffer[100]; // surely no pid is longer than 100
            int_str(child_pids[i], buffer);
            display_message(buffer);
            display_message("\n");
        }
        return;
    }
    if (strcmp(token_arr[0], "kill") == 0) {
        display_error("ERROR: Builtin failed: ", token_arr[0]);
        return;
    }
    if (strcmp(token_arr[0], "cd") == 0) {
        display_error("ERROR: Builtin failed: ", token_arr[0]);
        return;
    }
    if (strcmp(token_arr[0], "start-server") == 0) {
        display_error("ERROR: No port provided", "");
        return;
    }
    if (strcmp(token_arr[0], "close-server") == 0) {
        close_server(); // does close server need paramters?
        return;
    }
    if (strcmp(token_arr[0], "start-client") == 0) {
        display_error("ERROR: No port provided", "");
        display_error("ERROR: No hostname provided", "");
        return;
    }
    if (strcmp(token_arr[0], "send") == 0) {
        display_error("ERROR: No port provided", "");
        display_error("ERROR: No hostname provided", "");
        display_error("ERROR: No message provided", "");
        return;
    }
    if (strcmp(token_arr[0], "cat") == 0 || strcmp(token_arr[0], "wc") == 0) { // this is stupid
        display_error("ERROR: No input source provided", ""); // im doing it purely for the error (idk when this will ever display)_
        display_error("ERROR: Builtin failed: ", token_arr[0]);
        return;
    }
    exec_shell_command(token_arr, child_pids, child_count, bg);
}

void handle_two_args(char **token_arr) {
    if (strcmp(token_arr[0], "cat") == 0) {
        cat_func(token_arr[1], 1);
        return;
    }
    if (strcmp(token_arr[0], "wc") == 0) {
        wc_func(token_arr[1], 1);
        return;
    }
    if (strcmp(token_arr[0], "cd") == 0) {
        cd_func(token_arr[1]);
        return;
    }
    if (strcmp(token_arr[0], "start-server") == 0) {
        start_server(token_arr[1]);
        return;
    }
    if (strcmp(token_arr[0], "start-client") == 0) {
        display_error("ERROR: No hostname provided", "");
        return;
    }
    if (strcmp(token_arr[0], "send") == 0) {
        display_error("ERROR: No hostname provided", "");
        display_error("ERROR: No message provided", "");
        return;
    }
    if (strcmp(token_arr[0], "kill") == 0) {
        char *endptr;
        long pid = strtol(token_arr[1], &endptr, 10);
        // Check if the conversion was successful
        if (token_arr[1][0] == '\0' || *endptr != '\0' || pid <= 0) {
            // Conversion failed (either empty string or invalid characters)
            display_error("ERROR: The process does not exist", "");
            return;
        }

        for (size_t i = 0; i < child_count; i++) {
            if (child_pids[i] == (pid_t)pid) {
                if (kill(pid, SIGTERM) == -1) { // pid not found,
                    // Only print an error if it's NOT "No such process"
                    if (errno != ESRCH) {
                        perror("kill");
                    }
                }
                display_message("[");
                char buffer[MAX_CHILD_PROCESSES];
                int_str(i + 1, buffer);
                display_message(buffer);
                display_message("]+  Done ");
                display_message(cmd_arr[i]);
                display_message("\n");
                child_pids[i] = -1; // Mark as terminated
                if (cmd_arr[i] != NULL) { free(cmd_arr[i]); }
                cmd_arr[i] = NULL;  // Mark as unused
                return;
            }
        }
        if (kill(pid, SIGTERM) == -1) { // In case pid was from a different shell
            if (errno != ESRCH) {
                perror("kill");
            }
            display_error("ERROR: The process does not exist", "");
            return;
        }
    }
    exec_shell_command(token_arr, child_pids, child_count, bg);
}

void execute_normally(char **token_arr, size_t token_count) {
    bn_ptr builtin_fn = check_builtin(token_arr[0]);
    if (builtin_fn != NULL) {
        execute_builtin(builtin_fn, token_arr);
    }
    else if (strcmp("ls", token_arr[0]) == 0) {
        handle_ls(token_arr, token_count);
    }
    else if (token_count == 1) {
        handle_single_arg(token_arr);
    }
    else if (token_count == 2) {
        handle_two_args(token_arr);
    }
    else if (token_count == 3) {
        if (strcmp(token_arr[0], "send") == 0) {
            display_error("ERROR: No message provided", "");
            return;
        }
        // so far kill is the only command with 3 args that is not ls
        if (strcmp(token_arr[0], "kill") == 0) {
            char *endptr;
            long pid = strtol(token_arr[1], &endptr, 10);
            // Check conversion
            if (token_arr[1][0] == '\0' || *endptr != '\0') {
                // Conversion failed (either empty string or invalid characters)
                display_error("ERROR: The process does not exist", "");
                return;
            }
            if (pid <= 0) {
                display_error("ERROR: The process does not exist", "");
                return;
            }
            long signum = strtol(token_arr[2], &endptr, 10);
            if (token_arr[2][0] == '\0' || *endptr != '\0') {
                // Conversion failed
                display_error("ERROR: Invalid signal specified", "");
                return;
            }
            if (!(signum > 0 && signum < NSIG) || (signum >= SIGRTMIN && signum <= SIGRTMAX)) { // in linux
                display_error("ERROR: Invalid signal specified", "");
                return;
            }
            if (kill(pid, signum) == -1) {
                // Only print an error if it's NOT "No such process"
                if (errno != ESRCH) {
                    perror("kill");
                }
            }
            return;
        } else if (strcmp(token_arr[0], "start-client") == 0) { // TODO add error checking
            start_client(token_arr[1], token_arr[2]);
        }
    } else if (token_count >= 4) {
        char buffer[MAX_STR_LEN] = "";
        for (size_t i = 3; i < token_count; i++) {
            strcat(buffer, token_arr[i]);
            strcat(buffer, " ");
        }
        if (strcmp(token_arr[0], "send") == 0) {
            send_message(token_arr[1], token_arr[2], buffer); // portnumber, hostname, message
        }
    }
    else {
        exec_shell_command(token_arr, child_pids, child_count, bg);
    }
}

void execute_command(char **str) {
    // Tokenize the command string into arguments
    char *args[MAX_STR_LEN];
    size_t arg_count = 0;
    char *token = strtok(*str, " ");
    while (token != NULL) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL; // Null-terminate the argument list
    // Check for built-in
    bn_ptr builtin_fn = check_builtin(args[0]);
    if (builtin_fn != NULL) {
        execute_builtin(builtin_fn, args);
    }
    else if (strcmp("ls", args[0]) == 0) {
        handle_ls(args, arg_count);
    } else if (arg_count == 1) {
        handle_single_arg(args);
    } else if (arg_count == 2) {
        handle_two_args(args);
    } else {
        exec_shell_command(args, child_pids, child_count, bg);
    }
}

void execute_pipeline(char **pipe_arr) {
    // input is cat somefile | wc. pipe_arr is {cat somefile, wc, NULL}
    int num_commands = 0;
    while (pipe_arr[num_commands] != NULL) {
        num_commands++;
    }
    if (num_commands < 1) {
        display_error("ERROR: No commands to execute", "");
        return;
    }
    // Create n-1 pipes
    int pipefd[num_commands - 1][2];
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd[i]) == -1) {
            display_error("ERROR: Cannot create pipe", "");
            return;
        }
    }

    // Fork processes
    pid_t pids[num_commands];
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            display_error("ERROR: Failed to fork", "");
            return;
        } else if (pids[i] == 0) {
            if (i > 0) {
                // Redirect input from the previous pipe
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }
            if (i < num_commands - 1) {
                // Redirect output to the next pipe
                dup2(pipefd[i][1], STDOUT_FILENO);
            }

            // Close all pipe file descriptors
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // Execute the command
            execute_command(&pipe_arr[i]); // Pass the command to execute_command
            exit(EXIT_FAILURE); // If execute_command returns, exit with failure
        }
    }

    // Parent process: close all pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

void handle_pipe(char **token_arr, size_t token_count) {
    // string parsing goes here
    char **pipe_arr = parse_for_pipe(token_arr, token_count);
    if (pipe_arr[0] == NULL) {
        for (int i = 0; pipe_arr[i] != NULL; i++) {
            free(pipe_arr[i]);
        }
        free(pipe_arr);
    }
    if (pipe_arr) {
        execute_pipeline(pipe_arr);
        // Free the pipe_arr elements and the array itself
        for (int i = 0; pipe_arr[i] != NULL; i++) {
            free(pipe_arr[i]);
        }
        free(pipe_arr);
    }
}

void display_bg_process(int pid) {
    char buffer[100]; // surely no pid is greater than 100
    display_message("[");
    int_str(universal_child_count + 1, buffer);
    display_message(buffer);
    display_message("] ");
    int_str(pid, buffer);
    display_message(buffer);
    display_message("\n");
}

void sigint_handler(int signum) {
    (void)signum; // Avoid unused variable warning
    display_message("\nmysh$ ");
}

// TODO echo test | echo $y throws SEGV and memory issues if y DNE
// TODO deal with diplaying bg pipelines correctly.
// TODO get rid of "kill: (1912916): No such process" after termination another shell's process

// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc,
         __attribute__((unused)) char* argv[]) {
    // Ignore the SIGINT signal (Ctrl+C)
    signal(SIGINT, sigint_handler);
    int p = 0;
    char *prompt = "mysh$ ";
    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    while (1) {
        if (!p) {
          usleep(10 * 1000); // sleeps for 10 ms to show next mysh on new line
          display_message(prompt);
          p = 1;
        }


        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        struct timeval timeout = {0, 0};
        int activity = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select");
            continue;
        }

        if (check_server_activity()) {
            p = 0;
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
			p = 0;
            reap_zombies();
            int ret = get_input(input_buf);
            char *token_arr[MAX_STR_LEN] = {NULL};
            size_t token_count = tokenize_input(input_buf, token_arr);
            if (ret == 0) {
                free_tokens(token_arr);
                display_message("\n");
                break;
            }
            if (ret != -1 && (token_count != 0 && (strncmp("exit", token_arr[0], 4) == 0) && token_arr[0][4] == '\0')) {
                // Parent process
                stop_all_child_processes(child_pids, child_count);
                free_tokens(token_arr);
                break;
            }
            if (ret == -1) {
                free_tokens(token_arr);
                continue;
            }

            // Handle variable expansion in all tokens
            for (size_t i = 0; i < token_count; ++i) {
                // Treat as normal echo and not an expansion
                if (token_arr[i][0] == '$') {
                    if (token_arr[i][1] == ' ' || token_arr[i][1] == '\0') { continue; }
                    else {
                        char *expanded = expand_variables(token_arr[i]);
                        if (expanded) {
                            free(token_arr[i]);
                            token_arr[i] = expanded; // Replace with the expanded version
                        }
                    }
                }
            }
            bg = false;
            bpipe = false;
            for (size_t i = 0; i < token_count; i++) {
                if (strchr(token_arr[i], '|') != NULL) {
                    bpipe = true;
                }
                // Case 1: {"sleep", "1", "&", NULL})
                if (strcmp(token_arr[i], "&") == 0) {
                    if (i != token_count - 1) {
                        display_error("ERROR: Invalid position for & (must be at the end)", "");
                        break;
                    }
                    bg = true;
                    free(token_arr[i]);
                    token_arr[i] = NULL;
                }
                //Case 2: {"sleep", "1&", NULL})
                else {
                    char *and_ptr = strchr(token_arr[i], '&');
                    if (and_ptr != NULL) {
                        if (i != token_count - 1) {
                            display_error("ERROR: Invalid position for & (must be at the end)", "");
                            break;
                        }
                        bg = true;
                        *and_ptr = '\0'; // "1&" → "1")
                    }
                }
            }

            if (*token_arr != NULL && strchr(token_arr[0], '=') != NULL) {
                // Check for environment variable assignment
                if (!bpipe) {
                    char *equals_sign = strchr(token_arr[0], '=');
                    *equals_sign = '\0'; // Split the string into name and value
                    const char *var_name = token_arr[0];
                    char *var_value = equals_sign + 1;

                    int should_free = 0;
                    if (var_value[0] == '$') {
                        var_value = expand_variables(var_value);
                        should_free = 1;
                    }

                    if (strlen(var_name) == 0) {
                        display_error("Invalid environment variable assignment: ", token_arr[0]);
                        continue;
                    }
                    // Set the environment variable and check for cases like x1=$x0
                    set_env_var(var_name, var_value);
                    if (should_free == 1) {
                        free(var_value);
                    }
                    free_tokens(token_arr);
                    continue;
                }
            }

            // Command execution
            if (token_count < 1) { continue; }
            if (!bg) {
                if (bpipe) {
                    handle_pipe(token_arr, token_count);
                } else {
                    execute_normally(token_arr, token_count);
                }
            }
            else {
                pid_t pid = fork();
                if (pid < 0) {
                    // Fork failed
                    perror("fork");
                    free_tokens(token_arr);
                    exit(1);
                } else if (pid == 0) {
                    display_bg_process(getpid());
                    if (bpipe) {
                        handle_pipe(token_arr, token_count - 1);
                    }
                    else {
                        execute_normally(token_arr, token_count - 1);
                        if (strcmp(token_arr[0], "kill") == 0) {
                            reap_zombies();
                        }
                    }
                    free_tokens(token_arr);
                    exit(0);
                } else {
                    if (child_count < MAX_CHILD_PROCESSES) {
                        child_pids[child_count] = pid; // Add the child PID to the list
                        // Store the command in cmd_arr
                        char buffer[1024];
                        buffer[0] = '\0'; // Initialize buffer
                        for (size_t i = 0; i < token_count - 1; i++) {
                            strncat(buffer, token_arr[i], sizeof(buffer) - strlen(buffer) - 1);
                            if (i < token_count - 2) { // don't add space at the end
                                strncat(buffer, " ", 2);
                            }
                        }
                        cmd_arr[child_count] = strdup(buffer);
                        child_count++;
                        universal_child_count++;
                    } else {
                        display_error("Too many child processes. Cannot track more.\n", "");
                    }
                }
            }
            free_tokens(token_arr);
        }
    }
    free_env_vars();
    return 0;
}
