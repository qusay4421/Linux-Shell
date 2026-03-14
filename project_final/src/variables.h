#ifndef VARIABLES_H
#define VARIABLES_H

// Sets an environment variable
void set_env_var(const char *name, const char *value);

// Expands variables in a token (e.g., $VAR -> value)
char *expand_variables(const char *token);

// Frees all environment variables
void free_env_vars(void);

// replaces dollar signs in token with null characters and returns a list of char *
char **replace_dollars_with_null(const char *input, int *count);

#endif
