#include "file.h"
#include <stdio.h>
#include <stdlib.h>

// Get the Config Path
//
char* getConfigPath() {
  //Can modify this to Extent ~/.config.json
  const char *config_json = "config.json";

  return config_json;
}

char *read_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    fprintf(stderr, "Could not open file %s\n", filename);
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

// int main() {
//   const char *filename = "config.json";
//   char *json_string = read_file(filename);
//   if (!json_string) {
//     return EXIT_FAILURE;
//   }
//
//   printf("JSON file content:\n%s\n", json_string);
//
//   free(json_string);
//
//   return EXIT_SUCCESS;
// }
