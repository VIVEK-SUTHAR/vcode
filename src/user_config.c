#include "user_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Get the Config Path
char *getConfigPath() {
  char *config_json = "config.json";

  return config_json;
}

char *read_config_file() {
  char *config_file_path = getConfigPath();
  FILE *file = fopen(config_file_path, "r");
  if (!file) {
    fprintf(stderr, "Could not open file %s\n", config_file_path);
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(length + 1);
  if (!buffer) {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(file);
    return NULL;
  }

  fread(buffer, 1, length, file);
  buffer[length] = '\0';

  fclose(file);
  return buffer;
}
