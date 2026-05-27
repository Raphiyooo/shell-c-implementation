#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

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

void addCommand(char** full_path, char* path, char* command)
{
  *full_path = (char*) malloc(strlen(path) + 1 + strlen(command) + 1);
  if (!full_path)
    return;
  sprintf(*full_path, "%s/%s", path, command);
}

void locateExecutableFiles(char* command)
{
  char* paths_env = getenv("PATH");
  if (paths_env == NULL)
    return;
  // needed so i dont modify the system
  char* paths_env_copy = strdup(paths_env);

  char* save_paths = NULL;
  char* path = strtok_r(paths_env_copy, ":", &save_paths);
  while (path != NULL)
  {
    char* full_path = NULL;
    addCommand(&full_path, path, command);
    if (access(full_path, F_OK) == 0)
    {
      if (access(full_path, X_OK) == 0)
      {
        printf("%s is %s\n", command, full_path);
        free(paths_env_copy);
        free(full_path);
        return;
      }
    }
    free(full_path);
    path = strtok_r(NULL, ":", &save_paths);
  }
  free(paths_env_copy);
  printf("%s: not found\n", command);
}

void handleType(char* args)
{
  bool builtIn = isBuiltIn(args);
  if (builtIn)
    printf("%s is a shell builtin\n", args);
  else
  {
    locateExecutableFiles(args);
  }
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
