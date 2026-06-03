#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef _WIN32
  #define PATHSEP ";"
#else
  #define PATHSEP ":"
#endif

#ifdef _WIN32
  #define HOMEPATH "USERPROFILE"
#else
  #define HOMEPATH "HOME"
#endif

extern char** environ;

void handleEcho(char* args)
{
  printf("%s\n", args);
}

bool isBuiltIn(char* command)
{
  const char* built_ins[] = {
    "echo",
    "exit",
    "type",
    "pwd"
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

void executeProgram(char* full_path, char* tokenized_args_array[])
{
  pid_t pid;
  pid = fork();
  if (pid == -1)
    perror("Error while forking");
  else if (pid == 0)
  {
    if (execve(full_path, tokenized_args_array, environ) == -1)
    {
      perror("Could not execute execve");
      exit(1);
    }
  }
  else
  {
    int status;
    waitpid(pid, &status, 0);
  }
}

void buildArgsArrayCallExecute(char* first_word, char* args, char* full_path)
{
  size_t counter = 0;
  char* tokenized_args_array[64] = {NULL};
  tokenized_args_array[counter++] = first_word;

  char* save_args = NULL;
  char* token = strtok_r(args, " ", &save_args);
  while (token != NULL)
  {
    tokenized_args_array[counter++] = token;
    token = strtok_r(NULL, " ", &save_args);
  }
  tokenized_args_array[counter] = NULL;
  
  executeProgram(full_path, tokenized_args_array);
}

void handlePwd()
{
  char full_path_cur_dir[1024] = "";
  if (getcwd(full_path_cur_dir, sizeof(full_path_cur_dir)) == NULL)
    perror("Can't get current directory");
  else
    printf("%s\n", full_path_cur_dir);
}

void handleCd(char* args)
{
  char* home_path = NULL;
  if (strcmp(args, "~") == 0)
    home_path = getenv(HOMEPATH);
  else
    home_path = args;
  if (chdir(home_path) != 0)
    printf("cd: %s: No such file or directory\n", home_path);
  
}

int main(int argc, char* argv[])
{
  while (1)
  {
    setbuf(stdout, NULL);

    printf("$ ");
    char *line = NULL;
    size_t cap = 0;
    if (getline(&line, &cap, stdin) == -1)
      break;
    line[strlen(line) - 1] = '\0';

    char* line_copy = strdup(line);

    char* save_input = NULL;
    char* command = strtok_r(line_copy, " ", &save_input);
    if (command == NULL)
    {
      free(line_copy);
      continue;
    }
    // points to the first non input word in the line
    char* args = save_input;

    if (strcmp(command, "exit") == 0)
      break;
    else if (strcmp(command, "echo") == 0)
      handleEcho(args);
    else if (strcmp(command, "type") == 0)
      handleType(args);
    else if (strcmp(command, "pwd") == 0)
      handlePwd();
    else if (strcmp(command, "cd") == 0)
      handleCd(args);
    else
    {
      char* full_path = NULL;
      bool is_executable = locateExecutableFiles(command, &full_path);
      if (is_executable)
        buildArgsArrayCallExecute(command, args, full_path);
      else
        printf("%s: command not found\n", command);
    }

    free(line_copy);
    free(line);

    
    line = NULL;
  }
  return 0;
}
