#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
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

bool isSpecialChar(char c)
{
  const char* specialChars = "'\\\"$*? n_";
  if (strchr(specialChars, c) == NULL)
    return false;
  return true;
}

bool isSpecialCharWithinDoubleQuotes(char c)
{
  const char* specialChars = "\\\"$`n";
  if (strchr(specialChars, c) == NULL)
    return false;
  return true;
}

bool isBuiltIn(char* command)
{
  const char* built_ins[] = {
    "echo",
    "exit",
    "type",
    "pwd",
    "cd"
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

void handleType(char output[][1024], int amount_tokens)
{
  if (amount_tokens < 2)
    return;
  for (size_t i = 1; i < amount_tokens; i++)
  {
    char* full_path = NULL;
    bool builtIn = isBuiltIn(output[i]);
    if (builtIn)
    {
      printf("%s is a shell builtin\n", output[i]);
      return;
    }
    bool got_executable = locateExecutableFiles(output[i], &full_path);
    if (got_executable)
      printf("%s is %s\n", output[i], full_path);
    else
      printf("%s: not found\n", output[i]);

    if (full_path != NULL)
      free(full_path);
  }
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

void buildArgsArrayCallExecute(char output[][1024], char* full_path, int amount_tokens)
{
  char* arguments[10];
  for (size_t i = 0; i < amount_tokens; i++)
  {
    arguments[i] = output[i];
  }
  arguments[amount_tokens] = NULL;
  
  executeProgram(full_path, arguments);
}

void handlePwd()
{
  char full_path_cur_dir[1024] = "";
  if (getcwd(full_path_cur_dir, sizeof(full_path_cur_dir)) == NULL)
    perror("Can't get current directory");
  else
    printf("%s\n", full_path_cur_dir);
}

void handleCd(char output[][1024], int amount_tokens)
{
  char* home_path = NULL;
  if (amount_tokens < 2)
  {
    if (strcmp(output[1], "~") == 0)
      home_path = getenv(HOMEPATH);
    else
      home_path = output[1];
    if (chdir(home_path) != 0)
      printf("cd: %s: No such file or directory\n", home_path);
  }
  else
    perror("cd failed");

}

void trimSpaces(char trimmed[], const char* str)
{
  if (str == NULL) return;
  int idx = 0;
  while (*str != '\0')
  {
    if (!(*str == ' '))
      trimmed[idx++] = *str;
    else
      if ((idx > 0) && (trimmed[idx - 1] != ' '))
        trimmed[idx++] = ' ';
    str++;
  }
  trimmed[idx] = '\0';
}

void handleQuotes(char* args, char output[][1024], int* amount_tokens)
{
  int single_quote_ascii = '\'';
  int double_quote_ascii = '\"';
  int backslash_ascii = '\\';
  int token_idx = 0;
  int char_idx = 0;
  bool single_quote = false;
  bool double_quote = false;
  bool ignored = false;
  while (*args != '\0')
  {
    if (*args == single_quote_ascii || *args == double_quote_ascii)
    {
      if (*args == single_quote_ascii && double_quote == true)
      {
        output[token_idx][char_idx++] = '\'';
      }
      else if (*args == double_quote_ascii && single_quote == true)
      {
        output[token_idx][char_idx++] = '\"';
      }
      else
      {
        if (*args == single_quote_ascii)
          single_quote = !single_quote;
        else
          double_quote = !double_quote;
      }
    }
    else if (isspace(*args))
    {
      if (single_quote || double_quote)
        output[token_idx][char_idx++] = *args;
      else
      {
        if (char_idx > 0)
        {
          output[token_idx][char_idx] = '\0';
          token_idx++;
          char_idx = 0;
        }
      }
    }
    else if (*args == backslash_ascii)
    {
      if (single_quote)
        output[token_idx][char_idx++] = *args;
      else if (double_quote)
      {
        if (isSpecialCharWithinDoubleQuotes(*(args + 1)))
        {
          output[token_idx][char_idx++] = *(args + 1);
          args++;
        }
        else
          output[token_idx][char_idx++] = *args;
      }
      else
      {
        if (isSpecialChar(*(args + 1)))
        {
          output[token_idx][char_idx++] = *(args + 1);
          args++;
        }
        else
          output[token_idx][char_idx++] = *args;
      }
    }
    else
    {
      output[token_idx][char_idx++] = *args;
    }
    args++;
  }

  output[token_idx++][char_idx] = '\0';
  *amount_tokens = token_idx;
}

void handleEcho(char output[][1024], int amount_tokens)
{
  for (size_t i = 1; i < amount_tokens; i++)
  {
    printf("%s", output[i]);
    if (i < amount_tokens - 1)
      printf(" ");
  }

  printf("\n");
}


void handleCat(char output[][1024], int amount_tokens)
{
  for (size_t i = 1; i < amount_tokens; i++)
  {
    FILE* file_ptr = fopen(output[i], "r");
    if (file_ptr == NULL)
    {
      perror("file not found");
      continue;
    }
    size_t read_bytes = 1;
    char text_in_file[1024];
    while ((read_bytes = fread(text_in_file, sizeof(char), sizeof(text_in_file) - 1, file_ptr)) != 0)
    {
      text_in_file[read_bytes] = '\0';
      printf("%s", text_in_file);
    }
    fclose(file_ptr);
  }
  
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

    char output[10][1024];
    int amount_tokens = 0;
    handleQuotes(line, output, &amount_tokens);
    char* command = output[0];

    if (strcmp(command, "exit") == 0)
      break;
    else if (strcmp(command, "echo") == 0)
      handleEcho(output, amount_tokens);
    else if (strcmp(command, "type") == 0)
      handleType(output, amount_tokens);
    else if (strcmp(command, "pwd") == 0)
      handlePwd();
    else if (strcmp(command, "cd") == 0)
      handleCd(output, amount_tokens);
    else if (strcmp(command, "cat") == 0)
      handleCat(output, amount_tokens);
    else
    {
      char* full_path = NULL;
      bool is_executable = locateExecutableFiles(command, &full_path);
      if (is_executable)
        buildArgsArrayCallExecute(output, full_path, amount_tokens);
      else
        printf("%s: command not found\n", command);
    }

    free(line_copy);
    free(line);

    
    line = NULL;
  }
  return 0;
}
