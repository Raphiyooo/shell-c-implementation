#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

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

#define CTRLD_ASCII 4

extern char **environ;

bool isSpecialChar(char c)
{
  const char *specialChars = "'\\\"$*? n_123456789";
  if (strchr(specialChars, c) == NULL)
    return false;
  return true;
}

bool isSpecialCharWithinDoubleQuotes(char c)
{
  const char *specialChars = "\\\"$`n";
  if (strchr(specialChars, c) == NULL)
    return false;
  return true;
}

bool isBuiltIn(char *command)
{
  const char *built_ins[] = {
      "echo",
      "exit",
      "type",
      "pwd",
      "cd",
      "history"};
  size_t num_built_ins = sizeof(built_ins) / sizeof(built_ins[0]);
  for (size_t i = 0; i < num_built_ins; i++)
  {
    if (strcmp(command, built_ins[i]) == 0)
      return true;
  }
  return false;
}

void addCommand(char **full_path, char *path, char *command)
{
  *full_path = (char *)malloc(strlen(path) + 1 + strlen(command) + 1); // +1 for / and +1 for \0
  size_t len = strlen(path) + 1 + strlen(command) + 1;
  if (!(*full_path))
    return;
  snprintf(*full_path, len, "%s/%s", path, command);
}

bool locateExecutableFiles(char *args, char **full_path)
{
  char *paths_env = getenv("PATH");
  if (paths_env == NULL)
    return false;
  // needed so i dont modify the system
  char *paths_env_copy = strdup(paths_env);

  char *save_paths = NULL;
  char *path = strtok_r(paths_env_copy, PATHSEP, &save_paths);
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

void handleType(char output[][1024], size_t amount_tokens)
{
  if (amount_tokens < 2)
    return;
  for (size_t i = 1; i < amount_tokens; i++)
  {
    char *full_path = NULL;
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

void executeProgram(char *full_path, char *tokenized_args_array[])
{
  pid_t pid = fork();
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
    waitpid(pid, NULL, 0);
  }
}

void buildArgsArrayCallExecute(char output[][1024], char *full_path, size_t amount_tokens)
{
  char *arguments[10];
  for (size_t i = 0; i < amount_tokens; i++)
  {
    arguments[i] = output[i];
  }
  arguments[amount_tokens] = NULL;

  executeProgram(full_path, arguments);
}

void handlePwd(void)
{
  char full_path_cur_dir[1024] = "";
  if (getcwd(full_path_cur_dir, sizeof(full_path_cur_dir)) == NULL)
    perror("Can't get current directory");
  else
    printf("%s\n", full_path_cur_dir);
}

void handleCd(char output[][1024], size_t amount_tokens)
{
  char *home_path = NULL;
  if (amount_tokens > 0)
  {
    if (strcmp(output[1], "~") == 0 || amount_tokens == 1)
      home_path = getenv(HOMEPATH);
    else
      home_path = output[1];
    if (chdir(home_path) != 0)
      printf("cd: %s: No such file or directory\n", home_path);
  }
  else
    perror("cd failed");
}

void trimSpaces(char trimmed[], const char *str)
{
  if (str == NULL)
    return;
  size_t idx = 0;
  while (*str != '\0')
  {
    if (!(*str == ' '))
      trimmed[idx++] = *str;
    else if ((idx > 0) && (trimmed[idx - 1] != ' '))
      trimmed[idx++] = ' ';
    str++;
  }
  trimmed[idx] = '\0';
}

void handleQuotes(char *args, char output[][1024], size_t *amount_tokens)
{
  int single_quote_ascii = '\'';
  int double_quote_ascii = '\"';
  int backslash_ascii = '\\';
  int token_idx = 0;
  int char_idx = 0;
  bool single_quote = false;
  bool double_quote = false;

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

void handleEcho(char output[][1024], size_t amount_tokens)
{
  for (size_t i = 1; i < amount_tokens; i++)
  {
    printf("%s", output[i]);
    if (i < amount_tokens - 1)
      printf(" ");
  }

  printf("\n");
}

void handleCat(char output[][1024], size_t amount_tokens)
{
  for (size_t i = 1; i < amount_tokens; i++)
  {
    FILE *file_ptr = fopen(output[i], "r");
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

int redirect_output(char output[][1024], size_t *amount_tokens, int *target_fd, bool *redirected, int *saved_fd)
{
  bool append = false;
  for (size_t i = 1; i < (*amount_tokens) - 1; i++) // redirection operator cant be on first nor on last index
  {
    if (strcmp(output[i], ">") == 0 || strcmp(output[i], "1>") == 0 || strcmp(output[i], ">>") == 0 || strcmp(output[i], "1>>") == 0)
      *target_fd = STDOUT_FILENO;
    else if (strcmp(output[i], "2>") == 0 || strcmp(output[i], "2>>") == 0)
      *target_fd = STDERR_FILENO;
    else
      continue;
    // append
    if (strcmp(output[i], ">>") == 0 || strcmp(output[i], "1>>") == 0 || strcmp(output[i], "2>>") == 0)
      append = true;

    *saved_fd = dup(*target_fd);
    if (*saved_fd == -1)
    {
      perror("Dup failed");
      return 3;
    }
    int flags = O_WRONLY | O_CREAT;
    if (append)
      flags |= O_APPEND;
    else
      flags |= O_TRUNC;

    int fd = open(output[i + 1], flags, 0644);
    if (fd == -1)
    {
      perror("No such file or directory");
      close(*saved_fd);
      return 2;
    }
    if (dup2(fd, *target_fd) == -1)
    {
      perror("Error occurred while setting the file descriptors");
      close(fd);
      close(*saved_fd);
      return 1;
    }
    close(fd);
    *amount_tokens = i;
    output[i][0] = '\0';
    *redirected = true;
    break;
  }
  return 0;
}

void handleHistory(char output[][1024], size_t amount_tokens)
{
  // implementation by myself
  // char *endptr;
  // long convert_commands_show = strtol(output[1], &endptr, 10);
  // size_t amount_commands_shown = 0;
  // if (endptr == output[1]) // just history no int input
  //   amount_commands_shown = 0;
  // else if (*endptr != '\0')
  // {
  //   fprintf(stderr, "No valid number\n");
  //   return;
  // }
  // else
  // {
  //   amount_commands_shown = (size_t)convert_commands_show;
  //   amount_commands_shown--;
  // }
  // for (size_t i = amount_commands_shown; i < counting_input; i++)
  // {
  //   printf("%zu  %s\n", i, input_history[i]);
  // }
  // implementation with readline library
  if (history_length == 0)
    return;
  size_t start_index = history_base; // history get has 1 based indices
  size_t end_index = history_base + history_length - 1;
  if (amount_tokens == 1) // user just typed history
    start_index = 1;
  else if (amount_tokens == 2)
  {
    char* endptr = NULL;
    long convert_commands_show = strtol(output[1], &endptr, 10);
    if (*endptr != '\0')
    {
      fprintf(stderr, "Invalid number\n");
      return;
    }
    if ((size_t) convert_commands_show < (size_t) history_length)
      start_index = (size_t) history_length - (size_t) convert_commands_show + 1;
  }
  else
  {
    fprintf(stderr, "Invalid amount of tokens\n");
    return;
  }
  for (size_t i = start_index; i < end_index + 1; i++)
  {
    HIST_ENTRY* entry = history_get(i);
    if (entry != NULL && entry->line != NULL)
      printf("%zu  %s\n", i, entry->line);
  }
  
}

void addToHistory(char *input)
{
  // strcpy(input_history[(*counting_input)++], input);
  add_history(input);
}

void printTerminalState(const char *where)
{
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);

    printf("%s: ECHO=%s ICANON=%s\n",
           where,
           (current.c_lflag & ECHO) ? "ON" : "OFF",
           (current.c_lflag & ICANON) ? "ON" : "OFF");
}

int main(int argc, char *argv[])
{
  using_history();
  // set terminal into raw/non canonical mode
  struct termios old_attr;
  tcgetattr(STDIN_FILENO, &old_attr);
  struct termios new_attr = old_attr;
  new_attr.c_lflag &= ~ICANON; // disable canonical mode so i can process byte by byte
  new_attr.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
  while (1)
  {
    setbuf(stdout, NULL);

    printf("$ ");
    char c = '0';
    size_t capacity = 100;
    char* line = malloc(sizeof(char) * capacity);
    if (line == NULL)
    {
      tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
      return 1;
    }
    size_t length = 0;
    while (length < capacity - 1)
    {
      ssize_t ret_val = read(STDIN_FILENO, &c, sizeof(char));
      if (ret_val == -1)
      {
        fprintf(stderr, "Reading failed\n");
        tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
        return 1;
      }
      else if (c == CTRLD_ASCII) // eof
      {
        return 2;
        tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
      }
      else if (c == '\n' || c == '\r')
      {
        break;
      }
      else if (c == 127)
      {
        if (length > 0)
        {
          line[--length] = '\0';
          write(STDOUT_FILENO, "\b \b", sizeof(char) * 3); // \b move cursor one position left, print a space over old char, ove cursor one position left again
        }
      }
      else
      {
        line[length++] = c;
        fprintf(stderr, "[writing %d '%c']\n", (unsigned char)c, c);
        write(STDOUT_FILENO, &c, sizeof(char));
      }
    }
    line[length] = '\0';
    printf("\n");
    char *line_copy = strdup(line);
    // addToHistory(input_history, line_copy, &counting_input);
    add_history(line_copy);

    char output[10][1024];
    size_t amount_tokens = 0;
    handleQuotes(line, output, &amount_tokens);
    output[amount_tokens][0] = '\0';
    char *command = output[0];
    int saved_fd = -1;
    int target_fd = -1;
    bool redirected = false;
    int return_value = redirect_output(output, &amount_tokens, &target_fd, &redirected, &saved_fd);
    if (return_value != 0)
      return 1;

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
    else if (strcmp(command, "history") == 0)
    {
      handleHistory(output, amount_tokens);
    }
    else
    {
      char *full_path = NULL;
      bool is_executable = locateExecutableFiles(command, &full_path);
      if (is_executable)
        buildArgsArrayCallExecute(output, full_path, amount_tokens);
      else
        fprintf(stderr, "%s: command not found\n", command);
    }
    if (redirected)
    {
      fflush(stdout);
      fflush(stderr);
      dup2(saved_fd, target_fd);
      close(saved_fd);
    }

    free(line_copy);
    free(line);

    line = NULL;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
  return 0;
}