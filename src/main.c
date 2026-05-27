#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void handleEcho(char* args)
{
  printf("%s\n", args);
}

bool isBuiltIn(char* command)
{
  const char* built_ins[] = {
    "echo",
    "exit",
    "type"
  };
  size_t num_built_ins = sizeof(built_ins) / sizeof(built_ins[0]);
  for (size_t i = 0; i < num_built_ins; i++)
  {
    if (strcmp(command, built_ins[i]) == 0)
      return true;
  }
  return false;
}

void handleType(char* args)
{
  bool builtIn = isBuiltIn(args);
  if (builtIn)
    printf("%s is a shell builtin\n", args);
  else
    printf("%s: command not found\n", args);
}

int main(int argc, char *argv[])
{
  while (1)
  {
    setbuf(stdout, NULL);

    printf("$ ");
    char *line = NULL;
    size_t cap = 0;
    getline(&line, &cap, stdin);
    line[strlen(line) - 1] = '\0';

    char* save_input = NULL;
    char* input = strtok_r(line, " ", &save_input);
    if (input == NULL)
      continue;
    // points to the first non input word in the line
    char* args = save_input;

    if (strcmp(input, "exit") == 0)
      break;
    else if (strcmp(input, "echo") == 0)
      handleEcho(args);
    else if (strcmp(input, "type") == 0)
      handleType(args);
    else
      printf("%s: command not found\n", line);

    free(line);
  }
  return 0;
}
