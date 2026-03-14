#include "commands.h"
#include "io_helpers.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <limits.h> // has PATH_MAX
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#define MAXSIZE 256
#define MAX_CHILD_PROCESSES 100
#define MAX_CLIENTS 100



int server_fd = -1;
int client_fds[MAX_CLIENTS] = {0};
int client_count = 0;

void free_tokens(char **tokens) {
    // Check if tokens is NULL to prevent segmentation faults
    if (tokens == NULL) return;
    size_t i =0;
    while (tokens[i] != NULL){
        free(tokens[i]);
        i++;
    }
}

void int_str(int num, char *str) {
    int i = 0, is_negative = 0;

    // Handle negative numbers
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }

    // Extract digits in reverse order
    do {
        str[i++] = (num % 10) + '0';  // Convert digit to char
        num /= 10;
    } while (num > 0);

    // Add negative sign if needed
    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';  // Null-terminate the string

    // Reverse the string
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

void cat_func(char *filepath, int is_file){

    if (!is_file) {
		char buffer[128];
        clearerr(stdin); // clears EOF from stdin so it can be reused
    	while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        	printf("%s", buffer);
    	}
        return;
    }
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        display_error("ERROR: Cannot open file", "");
        display_error("ERROR: Builtin failed: ", "cat");
        return;
    }
    size_t r;
    char buf[200];
    while ((r = fread(buf, 1, 200, file)) > 0){
      	display_message(buf);
    }

    fclose(file);
    return;
}

void wc_func(char *filepath, int is_file) {

    FILE *file = fopen(filepath, "r");
    if (file == NULL && !is_file) {
        file = stdin;
    } else if (file == NULL) {
      display_error("ERROR: Cannot open file", "");
      display_error("ERROR: Builtin failed: ", "wc");
      return;
    }

    int ch;
    int word_count = 0, char_count = 0, newline_count = 0;
    bool is_word = false;
    clearerr(stdin); // clears EOF from stdin so it can be reused
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
          newline_count++;
        }
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
            if (is_word) {
                is_word = false;
                word_count++;
            }
        } else {
            is_word = true;
        }
        char_count++;
    }
    // If the file ends with a word (no trailing whitespace), count the last word
    if (is_word) {
        word_count++;
    }

    char buffer[20];
    int_str(word_count, buffer);
    display_message("word count ");
    display_message(buffer);
    display_message("\n");

    int_str(char_count, buffer);
    display_message("character count ");
    display_message(buffer);
    display_message("\n");

    int_str(newline_count, buffer);
    display_message("newline count ");
    display_message(buffer);
    display_message("\n");

    if (is_file) {
      fclose(file);
    }

}

void cd_func(char *path){
    if (path == NULL) {
        display_error("ERROR: Invalid path, path is NULL", "");
        return;
    }

    char path_acc[PATH_MAX];
    int depth = 0;
    char *directory;
    char *directory_arr[PATH_MAX];

    // check if path is relative or absolute
    if (path[0] != '/') {
        if (getcwd(path_acc, PATH_MAX) == NULL) {
            display_error("ERROR: Invalid path to cd into", "");
            return;
        }
        strcat(path_acc, "/");
        strcat(path_acc, path);
    } else {
        strcpy(path_acc, path);
    }

    directory = strtok(path_acc, "/");
    while (directory != NULL) {
      // check the .. ... .... logic and subtract from depth accordingly
        if (strcmp(directory, ".") == 0) {
            // do nothing, not removed for clarity
        } else if (strcmp(directory, "..") == 0) {
            if (depth > 0) {
                depth--;
            }
        } else if (strcmp(directory, "...") == 0) {
            if (depth > 1) {
                depth -= 2;
            } else {
                depth = 0;
            }
        } else if (strcmp(directory, "....") == 0) {
            if (depth > 2) {
                depth -= 3;
            } else {
                depth = 0;
            }
        } else {
            directory_arr[depth++] = directory;
        }
        directory = strtok(NULL, "/");
    }

    char final_path[PATH_MAX] = "/";
    for (int i = 0; i < depth; i++) {
        strcat(final_path, directory_arr[i]);
        if (i < depth - 1) {
            strcat(final_path, "/");
        }
    }

    if (chdir(final_path) != 0) {
        display_error("ERROR: Invalid directory", "");
        return;
    }
}

// piazza: Not having an integer after a --d is an error, for example.
// d = 0 shows a ., d < 0 throws an error
void ls_func(char *str) {
    // Show cwd
    if (strcmp(str, "") == 0) {
        list_directory(".", NULL, false, -1, 0);
        return;
    }

    // Flags
    bool recursive = false;
    int depth = -1;
    char *substring = NULL;
    char *token = strtok(str, " ");
    char *dir_path = NULL;

    while (token != NULL) {
        if (strcmp(token, "--rec") == 0) {
            recursive = true;
        } else if (strcmp(token, "--d") == 0) {
            token = strtok(NULL, " "); // Get the next token (the depth value)
            if (token != NULL) {
                depth = atoi(token);
            } else {
                display_error("ERROR: Invalid depth", "");
                return;
            }
        } else if (strcmp(token, "--f") == 0) {
            token = strtok(NULL, " "); // Get the next token (the substring value)
            if (token != NULL) {
                substring = token;
            }
        } else {
            if (dir_path == NULL) {
            	dir_path = token; // The last argument is assumed to be the directory path
            } else if (strncmp(dir_path, token, strlen(token)) == 0) {
                // do nothing
            }
            else {
                display_error("ERROR: Invalid path", "");
                display_error("ERROR: Builtin failed: ", "ls");
                return;
            }
        }
        token = strtok(NULL, " ");
    }
    if (depth != -1 && !recursive) {
        display_error("ERROR: --d requires --rec", "");
        return;
    } else if (depth == 1) {
        list_directory(".", NULL, false, -1, 0);
        return;
    }else if (dir_path == NULL) {
        list_directory(".", substring, recursive, depth, 0);
        return;
    }

    list_directory(dir_path, substring, recursive, depth, 0);
}

void list_directory(char *path, char *substring, bool recursive, int depth, int current_depth) {
    struct dirent *entry;
    struct stat statbuf;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        display_error("ERROR: Invalid path", "");
        display_error("ERROR: Builtin failed: ", "ls");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            if (substring != NULL && strcmp(substring, ".") == 0 && strcmp(substring, "..") == 0) {
            	display_message(entry->d_name);
            	display_message("\n");
            } else if (substring == NULL) {
                display_message(entry->d_name);
            	display_message("\n");
            }
            continue;
        }

        // Construct the full path
        char new_path[PATH_MAX];
        strcpy(new_path, path);
        strcat(new_path, "/");
        strcat(new_path, entry->d_name);

        if (stat(new_path, &statbuf) != 0) {
            display_error("ERROR: stat failed", new_path);
            continue;
        }
		if (substring == NULL && S_ISDIR(statbuf.st_mode)) {
        	display_message(entry->d_name);
        	display_message("\n");
        }

        // Recurse when recursive is true and the thing is a directory.
        if (recursive && S_ISDIR(statbuf.st_mode)) {
            if (depth == -1 || current_depth < depth) {
                list_directory(new_path, substring, recursive, depth, current_depth + 1);
            }
        }
         // Apply filtering only for files
        if (S_ISREG(statbuf.st_mode) && (substring == NULL || strstr(entry->d_name, substring) != NULL)) {
            display_message(entry->d_name);
            display_message("\n");
        }
    }
    closedir(dir);
}

void start_server(char *port_number) {
  	// check port number
    char *endptr;
    long port = strtol(port_number, &endptr, 10);
    if (*endptr != '\0' || port <= 0 || port > 65535) {
        display_error("ERROR: Invalid port number", "");
        return;
    }

    if (server_fd != -1) {
        display_error("ERROR: Server is already running", "");
        display_error("ERROR: Builtin failed: ", "start-server");
        return;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    fcntl(server_fd, F_SETFL, O_NONBLOCK); // from deepseek, makes it non-blocking

    // server setup
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(strtol(port_number, NULL, 10));

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        if (errno == EADDRINUSE) {
            display_error("ERROR: Server is already running", "");
            display_error("ERROR: Builtin failed: ", "start-server");
            return;
        }
        perror("bind");
        close(server_fd);
        server_fd = -1;
        return;
    }

    if (listen(server_fd, MAX_CLIENTS)) {
        perror("listen");
        close(server_fd);
        server_fd = -1;
        return;
    }
// 	  test print
//    display_message("Server started on port ");
//    display_message(port_number);
//    display_message("\n");
}

void close_server() {
    if (server_fd == -1) {
        display_error("ERROR: No server is running", "");
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] > 0) {
            close(client_fds[i]);
            client_fds[i] = 0;
        }
    }
    client_count = 0;
    close(server_fd);
    server_fd = -1;
}

int check_server_activity() {
    if (server_fd == -1) return 0;

    fd_set readfds;
    struct timeval timeout = {0, 0}; // deepseek
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);
    int max_sd = server_fd;

    // Add all existing clients to the set
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] > 0) {
            FD_SET(client_fds[i], &readfds);
            if (client_fds[i] > max_sd) {
                max_sd = client_fds[i];
            }
        }
    }

    int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
    if (activity < 0) {
        perror("select");
        return 0;
    }

    // Check for new connection
    if (FD_ISSET(server_fd, &readfds)) {
        int new_socket = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (new_socket >= 0) {
            fcntl(new_socket, F_SETFL, O_NONBLOCK);

            // Add to first available slot
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_fds[i] == 0) {
                    client_fds[i] = new_socket;
                    client_count++;
                    break;
                }
            }
        }
    }

    int success = 0;
    // Check existing clients for data
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_fds[i] > 0 && FD_ISSET(client_fds[i], &readfds)) {
            char *message = NULL;
            size_t message_size = 0;
            int found_newline = 0;

            while (!found_newline) {
                char chunk[1024];  // Temporary chunk buffer
                int bytes_read = read(client_fds[i], chunk, sizeof(chunk));

                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        // Client disconnected
                        close(client_fds[i]);
                        client_fds[i] = 0;
                        client_count--;
                    }
                    break;
                }

                // Check for newline in the chunk
                char *newline_pos = NULL;
                for (int j = 0; j < bytes_read; j++) {
                    if (chunk[j] == '\n') {
                        // Check for \r\n (network newline)
                        if (j > 0 && chunk[j-1] == '\r') {
                            newline_pos = &chunk[j-1];
                        } else {
                            newline_pos = &chunk[j];
                        }
                        found_newline = 1;
                        break;
                    }
                }

                // Calculate how much to keep (up to newline if found)
                size_t keep_bytes = found_newline ? (newline_pos - chunk) : bytes_read;

                // Reallocate message buffer
                char *new_buffer = realloc(message, message_size + keep_bytes + 1);
                if (!new_buffer) {
                    free(message);
                    perror("realloc");
                    close(client_fds[i]);
                    client_fds[i] = 0;
                    client_count--;
                    break;
                }
                message = new_buffer;

                // Append chunk to message
                memcpy(message + message_size, chunk, keep_bytes);
                message_size += keep_bytes;
                message[message_size] = '\0';

                if (found_newline) {
                    // Display the complete message
                    display_message(message);
                    display_message("\n");
                    success = 1;
                    free(message);
                    break;
                }
            }

            if (!found_newline && message) {
                // If we exited loop without finding newline but have data,
                // display it anyway (incomplete message)
                display_message(message);
                display_message("\n");
                success = 1;
                free(message);
            }
        }
    }
    return success;
}

void send_message(char *port_number, char *host_name, char *message) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in peer;
    peer.sin_family = AF_INET;
    peer.sin_port = htons(strtol(port_number, NULL, 10));

    if (inet_pton(AF_INET, host_name, &peer.sin_addr) < 0) {
        perror("inet_pton");
        close(socket_fd);
        return;
    }

    if (connect(socket_fd, (struct sockaddr *)&peer, sizeof(peer))) {
        perror("connect");
        close(socket_fd);
        return;
    }

    // Ensure message ends with newline
    size_t msg_len = strlen(message);
    int needs_newline = (msg_len == 0) || (message[msg_len-1] != '\n');

    if (needs_newline) {
        // Allocate space for message + \n
        char *full_message = malloc(msg_len + 2);
        if (!full_message) {
            perror("malloc");
            close(socket_fd);
            return;
        }
        strcpy(full_message, message);
        strcat(full_message, "\n");
        write(socket_fd, full_message, msg_len + 1);
        free(full_message);
    } else {
        write(socket_fd, message, msg_len);
    }

    close(socket_fd);
}

void start_client(char *port_number, char *host_name) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        return;
    }

    struct sockaddr_in peer;
    peer.sin_family = AF_INET;
    peer.sin_port = htons(strtol(port_number, NULL, 10));

    if (inet_pton(AF_INET, host_name, &peer.sin_addr) < 0) {
        perror("inet_pton");
        close(socket_fd);
        return;
    }

    if (connect(socket_fd, (struct sockaddr *)&peer, sizeof(peer))) {
        perror("connect");
        close(socket_fd);
        return;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    FD_SET(socket_fd, &fdset);

    while (1) {
        fd_set read_fds = fdset;
        if (select(socket_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char *input = NULL;
            size_t input_size = 0;
            ssize_t bytes_read = getline(&input, &input_size, stdin);

            if (bytes_read <= 0) {
                free(input);
                break;
            }

            write(socket_fd, input, bytes_read);
            free(input);
        }

        if (FD_ISSET(socket_fd, &read_fds)) {
            char *message = NULL;
            size_t message_size = 0;
            int found_newline = 0;

            while (!found_newline) {
                char chunk[1024];
                int bytes_read = read(socket_fd, chunk, sizeof(chunk));

                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        // Server disconnected
                        display_message("Server disconnected\n");
                    }
                    free(message);
                    close(socket_fd);
                    return;
                }

                // Check for newline
                char *newline_pos = NULL;
                for (int j = 0; j < bytes_read; j++) {
                    if (chunk[j] == '\n') {
                        if (j > 0 && chunk[j-1] == '\r') {
                            newline_pos = &chunk[j-1];
                        } else {
                            newline_pos = &chunk[j];
                        }
                        found_newline = 1;
                        break;
                    }
                }

                size_t keep_bytes = found_newline ? (newline_pos - chunk) : bytes_read;
                char *new_buffer = realloc(message, message_size + keep_bytes + 1);
                if (!new_buffer) {
                    free(message);
                    perror("realloc");
                    close(socket_fd);
                    return;
                }
                message = new_buffer;
                memcpy(message + message_size, chunk, keep_bytes);
                message_size += keep_bytes;
                message[message_size] = '\0';

                if (found_newline) {
                    write(STDOUT_FILENO, message, message_size);
                    write(STDOUT_FILENO, "\n", 1);
                    free(message);
                    break;
                }
            }
        }
    }

    close(socket_fd);
}

void execute_builtin(bn_ptr builtin_fn, char **token_arr) {
    ssize_t err = builtin_fn(token_arr);
    if (err == -1) {
        display_error("ERROR: Builtin failed: ", token_arr[0]);
    }
}

void exec_shell_command(char **token_arr, pid_t *child_pids, size_t child_count, bool bg) {
    char bin_path[PATH_MAX] = "/bin/";
    char user_bin_path[PATH_MAX] = "/usr/bin/";
    strncat(bin_path, token_arr[0], sizeof(bin_path) - strlen(bin_path) - 1);
    strncat(user_bin_path, token_arr[0], sizeof(user_bin_path) - strlen(user_bin_path) - 1);
    pid_t pid;
    pid = fork();  // to not exit out of mysh
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
    	execv(bin_path, token_arr);
		execv(user_bin_path, token_arr);
        // reached if execv does not work
        display_error("ERROR: Unknown command: ", token_arr[0]);
        display_error("ERROR: Builtin failed: ", token_arr[0]);
        exit(1);
    } else {
        // Parent process
        if (child_count < MAX_CHILD_PROCESSES) {
            child_pids[child_count++] = pid; // Add the child PID to the list
        } else {
            display_message("Too many processes, cannot track more.\n");
        }
        // Wait for the child process to finish
        int status;
        wait(&status);
        if (!WIFEXITED(status)) {
            display_error("Child process terminated abnormally\n", "");
        }
        if (bg) {exit(0);} // if this was a background process, parent must exit as well
    }
}
// FROM DEEPSEEK
char *strip_spaces_around_pipe(const char *input) {
    if (input == NULL) {
        return NULL; // Handle NULL input
    }

    // Find the position of the pipe character
    const char *pipe_pos = strchr(input, '|');
    if (pipe_pos == NULL) {
        return strdup(input); // No pipe found, return a copy of the input
    }

    // Check if the pipe is at the end of the string
    if (*(pipe_pos + 1) == '\0') {
        return NULL; // Pipe is at the end, return NULL
    }

    // Allocate memory for the result
    size_t input_len = strlen(input);
    char *result = (char *)malloc(input_len + 1); // +1 for null terminator
    if (result == NULL) {
        return NULL; // Memory allocation failed
    }

    // Copy characters before the pipe
    const char *start = input;
    const char *end = pipe_pos;
    while (start < end && isspace(*start)) {
        start++; // Skip leading spaces before the pipe
    }
    size_t before_len = end - start;
    memcpy(result, start, before_len);

    // Add the pipe character
    result[before_len] = '|';

    // Copy characters after the pipe
    start = pipe_pos + 1;
    end = input + input_len;
    while (start < end && isspace(*start)) {
        start++; // Skip leading spaces after the pipe
    }
    size_t after_len = end - start;
    memcpy(result + before_len + 1, start, after_len);

    // Null-terminate the result
    result[before_len + 1 + after_len] = '\0';

    return result;
}

char **parse_for_pipe(char **tokens, size_t token_count) {
    // reconstruct string
    char buffer[1024];
    for (size_t i = 0; i < token_count; i++) {
        strncat(buffer, tokens[i], sizeof(buffer) - strlen(buffer) - 1);
        if (i < token_count) { // dont add at the end
        	strncat(buffer, " ", 2);
        }
    }
    char **pipe_arr = malloc((token_count + 128) * sizeof(char *)); // MALLOC
    if (pipe_arr == NULL) {
        perror("malloc");
        exit(1);
    }
    char *cleaned = strip_spaces_around_pipe(buffer);
    char *pipe_token = strtok(cleaned, "|");
    int i = 0;
    while (pipe_token != NULL) {
        pipe_arr[i] = strdup(pipe_token); // need to free all pipe elements
        pipe_token = strtok(NULL, "|");
        i++;
    }
	pipe_arr[i] = NULL;
    free(cleaned);
    return pipe_arr;
}
