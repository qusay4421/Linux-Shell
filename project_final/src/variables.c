#include "variables.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    char *name;
    char *value;
    struct Node *next;
} Node;

static Node *head = NULL;
char *copy;

void set_env_var(const char *name, const char *value) {
    // Check for an existing variable
    Node *current = head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // Update value
            free(current->value);
            current->value = strdup(value); // Allocate memory for the new value
            return;
        }
        current = current->next;
    }

    // Variable not found; add a new node
    Node *new_node = malloc(sizeof(Node));
    if (!new_node) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    new_node->name = strdup(name);
    new_node->value = strdup(value);
    new_node->next = head;
    head = new_node;
}

char *expand_variables(const char *token) {
    if (token[0] != '$' || (token[0] == '$' && token[1] == ' ')) {
        return strdup(token); // No variable to expand
    }

    int count; // number of variable expansions
    char **result = replace_dollars_with_null(token, &count);
    if (result == NULL) {
        return strdup(""); // No variables found
    }
    // Allocate a buffer to build the expanded string
    char *expanded = (char *)malloc(count * 128); // Allocate needed space
    if (expanded == NULL) {
        free(result);
        return strdup("");
    }
    expanded[0] = '\0'; // Initialize the buffer

    // Iterate through the result array and expand variables
        int num_chars = 0;
    for (int i = 0; i < count; i++) {
        Node *current = head;
        int found = 0;

        // Search for the variable in the linked list
        while (current != NULL) {
            if (strcmp(current->name, result[i]) == 0) {
              // 122 characters is the max that should be printed.
                if (num_chars + strlen(current->value) <= 122) {
                        strcat(expanded, current->value); // Append the expanded value
                } else {
                    strncat(expanded, current->value, 122 - num_chars);
                }
                num_chars += strlen(current->value);
                found = 1;
                break;
            }
            current = current->next;
        }
        if (found == 0) {
            // If the variable is not found
            return("");
        }
    }
    // Free the allocated memory for result
    free(copy);
    free(result);
    return expanded;
}

// Function to replace '$' with '\0' and return a list of pointers
char **replace_dollars_with_null(const char *input, int *count) {
    if (input == NULL) {
        *count = 0;
        return NULL;
    }

    // Create a copy of the input string to modify
    copy = strdup(input);
    if (copy == NULL) {
        *count = 0;
        return NULL; // Memory allocation failed
    }

    // Count the number of '$' characters to determine the size of the result list
    int num_dollars = 0;
    for (char *p = copy; *p != '\0'; p++) {
        if (*p == '$') {
            num_dollars++;
        }
    }

    // Allocate memory for the list of pointers
    char **result = (char **)malloc(num_dollars * sizeof(char *));
    if (result == NULL) {
        *count = 0;
        free(copy); // Free the copied string
        return NULL; // Memory allocation failed
    }

    // Replace '$' with '\0' and populate the result list
    int index = 0;
    for (char *p = copy; *p != '\0'; p++) {
        if (*p == '$') {
            *p = '\0'; // Replace '$' with '\0'
            result[index++] = p + 1; // Point to the character after the '$'
        }
    }

//  Debug: Print the contents of the result array
//    printf("Debug: Result array contents:\n");
//    for (int i = 0; i < num_dollars; i++) {
//        printf("  result[%d] = %p (points to: \"%s\")\n", i, (void *)result[i], result[i]);
//    }

    *count = num_dollars;
    return result;
}

// Free all environment variables
void free_env_vars(void) {
    Node *current = head;
    while (current != NULL) {
        Node *next = current->next;
        free(current->name);
        free(current->value);
        free(current);
        current = next;
    }
    head = NULL;
}
