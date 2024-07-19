
// file.h

#ifndef FILE_H
#define FILE_H

typedef struct {
  char *welcome_message;
  short int tab_stop;
} UserConfig;

char *read_config_file();

UserConfig load_user_config();
#endif // FILE_H
