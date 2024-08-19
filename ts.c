#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tree_sitter/api.h>
extern TSLanguage *tree_sitter_c();

char *read_file(char *filepath) {
  FILE *current_file_pointer = fopen(filepath, "r");
  if (!current_file_pointer) {
    exit(EXIT_FAILURE);
  }
  fseek(current_file_pointer, 0, SEEK_END);
  long size = ftell(current_file_pointer);
  rewind(current_file_pointer);
  char *file_content = malloc(size + 1);
  if (!file_content) {
    perror("Failed to allocate memory");
    exit(EXIT_FAILURE);
  }
  size_t read_size = fread(file_content, 1, size, current_file_pointer);
  if (read_size != size) {
    perror("Failed to read file");
    exit(EXIT_FAILURE);
  }
  file_content[size] = '\0';
  fclose(current_file_pointer);
  return file_content;
}

void traverse_tree(TSNode node, const char *source_code) {
  const char *node_type = ts_node_type(node);
  uint32_t start_byte = ts_node_start_byte(node);
  uint32_t end_byte = ts_node_end_byte(node);

  printf("Node type: %s\n %.*s\n", node_type, end_byte - start_byte,
         source_code + start_byte);
  for (uint32_t i = 0; i < ts_node_child_count(node); ++i) {
    TSNode child = ts_node_child(node, i);
    traverse_tree(child, source_code);
  }
}

int main() {
  const char *source_file = read_file("ts.c");
  // const char *source_file = "#include <stdio.h>\n int main(int argc, char "
  //                           "*argv[]){ printf('Hello'); return 0;}";
  TSParser *parser = ts_parser_new();
  TSLanguage *language = tree_sitter_c();

  ts_parser_set_language(parser, language);

  TSTree *tree =
      ts_parser_parse_string(parser, NULL, source_file, strlen(source_file));

  TSNode root_node = ts_tree_root_node(tree);
  traverse_tree(root_node, source_file);

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return 0;
}
