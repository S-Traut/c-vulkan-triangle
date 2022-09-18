#ifndef BRL_FILE
#define BRL_FILE

#include <stdio.h>
#include <stdlib.h>

typedef struct brl_file
{
  char *data;
  size_t size;
} brl_file;

/**
 * You must free the data pointer after reading the file.
 **/
brl_file brl_read(char *file_path)
{
  FILE *file = fopen(file_path, "rb");
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *data = (char *)malloc(sizeof(char) * size + 1);
  fread(data, size, size, file);
  fclose(file);

  return (brl_file){
      .data = data,
      .size = size,
  };
}

void brl_file_close(brl_file file)
{
  free(file.data);
}

#endif