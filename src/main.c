#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef _WIN32
  #define PATHSEP ";"
#else
  #define PATHSEP ":"
#endif

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
  if (!(*full_path))
    return;
  sprintf(*full_path, "%s/%s", path, command);
}

bool locateExecutableFiles(char* args, char** full_path)
{
  char* paths_env = getenv("PATH");
  if (paths_env == NULL)
    return false;
  // needed so i dont modify the system
  char* paths_env_copy = strdup(paths_env);

  char* save_paths = NULL;
  char* path = strtok_r(paths_env_copy, PATHSEP, &save_paths);
  while (path != NULL)
  {
    if (*full_path != NULL)
    {
      free(*full_path);
      *full_path = NULL;
    }
    addCommand(full_path, path, args);
    if (access(*full_path, F_OK) == 0)
    {
      if (access(*full_path, X_OK) == 0)
      {
        free(paths_env_copy);
        return true;
      }
    }
    path = strtok_r(NULL, PATHSEP, &save_paths);
  }
  if (*full_path != NULL)
  {
    free(*full_path);
    *full_path = NULL;
  }

  free(paths_env_copy);
  return false;
}

void handleType(char* args)
{
  char* full_path = NULL;
  bool builtIn = isBuiltIn(args);
  bool got_executable = locateExecutableFiles(args, &full_path);
  if (builtIn)
    printf("%s is a shell builtin\n", args);
  else if (got_executable)
    printf("%s is %s\n", args, full_path);
  else
    printf("%s: not found\n", args);

  if (full_path != NULL)
    free(full_path);
}

void executeProgram(char* system_path)
{
  int succeed = system(system_path);
  if (succeed != 0)
    return; // if not 0 it didnt succeed
}

bool handleExternalPrograms(char* args)
{
  char* full_path = NULL;
  bool found = locateExecutableFiles(args, &full_path);

  if (found)
  {
    char* system_path = NULL;
    sprintf(system_path, "%s %s", full_path, args);
    executeProgram(system_path);
    free(system_path);
  }
  else
    return false;

  if (full_path != NULL)
    free(full_path);
  return true;
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
    char* command = strtok_r(line, " ", &save_input);
    if (command == NULL)
      continue;
    // points to the first non input word in the line
    char* args = save_input;

    if (strcmp(command, "exit") == 0)
      break;
    else if (strcmp(command, "echo") == 0)
      handleEcho(args);
    else if (strcmp(command, "type") == 0)
      handleType(args);
    else
    {
      if (handleExternalProgams(args))
        continue;
      else
        printf("%s: command not found\n", line);
    }

    free(line);
  }
  return 0;
}
