    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <stdio.h>

    #include "io_helpers.h"


    // ===== Output helpers =====

    /* Prereq: str is a NULL terminated string
     */
    void display_message(char *str) {
        write(STDOUT_FILENO, str, strnlen(str, MAX_STR_LEN));
    }


    /* Prereq: pre_str, str are NULL terminated string
     */
    void display_error(char *pre_str, char *str) {
        write(STDERR_FILENO, pre_str, strnlen(pre_str, MAX_STR_LEN));
        write(STDERR_FILENO, str, strnlen(str, MAX_STR_LEN));
        write(STDERR_FILENO, "\n", 1);
    }


    // ===== Input tokenizing =====

    /* Prereq: in_ptr points to a character buffer of size > MAX_STR_LEN
     * Return: number of bytes read
     */
    ssize_t get_input(char *in_ptr) { //DEEPSEEK REFACTORED IT
        fd_set readfds;
        struct timeval timeout = {0, 0}; // Non-blocking check
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        // Check if there's input available
        int activity = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("select");
            return -1;
        }
        if (activity == 0) {
            return 0; // No input available
        }

        // If we get here, there's input available
        int retval = read(STDIN_FILENO, in_ptr, MAX_STR_LEN+1);
        int read_len = retval;
        if (retval == -1) {
            read_len = 0;
        }
        if (read_len > MAX_STR_LEN) {
            read_len = 0;
            retval = -1;
            write(STDERR_FILENO, "ERROR: input line too long\n", strlen("ERROR: input line too long\n"));
            int junk = 0;
            while((junk = getchar()) != EOF && junk != '\n');
        }
        in_ptr[read_len] = '\0';
        return retval;
    }

    /* Prereq: in_ptr is a string, tokens is of size >= len(in_ptr)
     * Warning: in_ptr is modified
     * Return: number of tokens.
     */
    size_t tokenize_input(char *in_ptr, char **tokens) {
        char *curr_ptr = strtok(in_ptr, DELIMITERS);
        size_t token_count = 0;

        int num = 128 - 1;

        while (curr_ptr != NULL) {

            tokens[token_count] = malloc(128*sizeof(char));

            strncpy(tokens[token_count], curr_ptr, num);
            tokens[token_count][num] = '\0';

            curr_ptr = strtok(NULL, DELIMITERS);
            token_count++;
        }
        tokens[token_count] = NULL;

        return token_count;
    }
