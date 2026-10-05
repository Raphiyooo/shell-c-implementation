#define _POSIX_C_SOURCE 200809L

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
#include <dirent.h>

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
#define MAX_MATCHES 100

extern char **environ;

typedef struct DeclareVariable
{
  char name[1024];
  char type[1024];
} DeclareVariable;

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
      "history",
      "declare"};
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
    char *endptr = NULL;
    long convert_commands_show = strtol(output[1], &endptr, 10);
    if (*endptr != '\0')
    {
      fprintf(stderr, "Invalid number\n");
      return;
    }
    if ((size_t)convert_commands_show < (size_t)history_length)
      start_index = (size_t)history_length - (size_t)convert_commands_show + 1;
  }
  else if (amount_tokens == 3)
  {
    if (strcmp(output[1], "-w") == 0)
    {
      int fd = open(output[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1)
      {
        fprintf(stderr, "open");
        return;
      }
      for (size_t i = start_index; i < end_index + 1; i++)
      {
        HIST_ENTRY *entry = history_get(i);
        if (entry != NULL && entry->line != NULL)
          write(fd, entry->line, strlen(entry->line));
        write(fd, "\n", sizeof(char));
      }
      close(fd);
    }
    else if (strcmp(output[1], "-r") == 0)
    {
      int fd = open(output[2], O_RDONLY, 0644);
      if (fd == -1)
      {
        fprintf(stderr, "open");
        return;
      }
      char buffer[100];
      char line[100];
      size_t line_length = 0;
      ssize_t bytes_read;
      while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
      {
        for (size_t i = 0; i < (size_t)bytes_read; i++)
        {
          if (buffer[i] == '\n')
          {
            line[line_length] = '\0';
            add_history(line);
            line_length = 0;
          }
          else
            line[line_length++] = buffer[i];
        }
      }
      if (line_length > 0)
      {
        line[line_length] = '\0';
        add_history(line);
      }
      close(fd);
    }
    else if (strcmp(output[1], "-a") == 0)
    {
      int fd = open(output[2], O_RDWR | O_APPEND, 0644);
      if (fd == -1)
      {
        fprintf(stderr, "open");
        return;
      }
      off_t bytes_moved = lseek(fd, 0, SEEK_END);
      if (bytes_moved > 0) // check if it moved, else it means file is empty
      {
        lseek(fd, -1, SEEK_END); // this is possible since i tested it before
        char last_char;
        read(fd, &last_char, sizeof(char));
        if (last_char != '\n')
          write(fd, "\n", sizeof(char));
      }

      for (size_t i = start_index; i < end_index + 1; i++)
      {
        HIST_ENTRY *entry = history_get(i);
        if (entry != NULL && entry->line != NULL)
          write(fd, entry->line, strlen(entry->line));
        write(fd, "\n", sizeof(char));
      }
      close(fd);
    }
    return;
  }
  else
  {
    fprintf(stderr, "Invalid amount of tokens\n");
    return;
  }
  for (size_t i = start_index; i < end_index + 1; i++)
  {
    HIST_ENTRY *entry = history_get(i);
    if (entry != NULL && entry->line != NULL)
      printf("  %zu  %s\n", i, entry->line);
  }
}

int loadHistory(char history_path[])
{
  char *environment = getenv(HOMEPATH);
  if (environment == NULL)
  {
    fprintf(stderr, "Couldnt load environment\n");
    return 1;
  }
  snprintf(history_path, 1024, "%s/.customshell_history", environment);

  int fd = open(history_path, O_CREAT | O_RDONLY, 0600); // only user can read/write
  if (fd == -1)
  {
    fprintf(stderr, "open");
    return 1;
  }
  char buffer[100];
  char line[100];
  size_t line_length = 0;
  ssize_t bytes_read;
  while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
  {
    for (size_t i = 0; i < (size_t)bytes_read; i++)
    {
      if (buffer[i] == '\n')
      {
        line[line_length] = '\0';
        add_history(line);
        line_length = 0;
      }
      else
        line[line_length++] = buffer[i];
    }
  }
  if (line_length > 0)
  {
    line[line_length] = '\0';
    add_history(line);
  }
  close(fd);
  return 0;
}

void addToHistory(char *input)
{
  // strcpy(input_history[(*counting_input)++], input);
  add_history(input);
}

int readUserInput(char *line, size_t *length, size_t capacity)
{
  char c = '0';
  size_t history_index = 0;
  bool pressed_tab = false;
  while (*length < capacity - 1)
  {
    ssize_t ret_val = read(STDIN_FILENO, &c, sizeof(char));
    if (ret_val == -1)
    {
      fprintf(stderr, "Reading failed\n");
      return 1;
    }
    else if (c == CTRLD_ASCII) // eof
      return 1;
    else if (c == '\n' || c == '\r')
      break;
    else if (c == 127) // backwards
    {
      if (*length > 0)
      {
        line[--(*length)] = '\0';
        write(STDOUT_FILENO, "\b \b", sizeof(char) * 3); // \b move cursor one position left, print a space over old char, ove cursor one position left again
      }
    }
    else if (c == 27) // add arrow up/down history
    {
      read(STDIN_FILENO, &c, sizeof(char));
      if (c == '[')
      {
        read(STDIN_FILENO, &c, sizeof(char));
        if (c == 'A')
        {
          // arrow up
          if (history_index < (size_t)history_length)
          {
            history_index++;
            write(STDOUT_FILENO, "\r\033[K$ ", 6);
          }
          else
            continue;
          HIST_ENTRY *list = history_get(history_base + history_length - history_index);
          if (list != NULL)
            write(STDOUT_FILENO, list->line, sizeof(char) * (strlen(list->line)));
          strcpy(line, list->line); // since list->line is a normal c string, i can omit putting the \0
          *length = strlen(list->line);
        }
        else if (c == 'B')
        {
          if (history_index > 1)
          {
            history_index--;
            write(STDOUT_FILENO, "\r\033[K$ ", 6);
          }
          else
          {
            write(STDOUT_FILENO, "\r\033[K$ ", 6); // \r move cursor to beginning, rest is to clear terminal from cursor to end of line
            history_index = 0;
            *length = 0;
            continue;
          }
          HIST_ENTRY *list = history_get(history_base + history_length - history_index);
          if (list != NULL)
            write(STDOUT_FILENO, list->line, sizeof(char) * (strlen(list->line)));
          strcpy(line, list->line); // since list->line is a normal c string, i can omit putting the \0
          *length = strlen(list->line);
        }
        else
          fprintf(stderr, "Unknown input\n"); // means input started with Esc[ but something different followed
      }
    }
    else if (c == '\t')
    {
      size_t length_before_argument = 0;
      size_t index = 0;
      char argument[1024];
      for (size_t i = 0; i < *length; i++)
      {
        if (i == 0)
        {
          while (line[i++] != ' ') // skip
            length_before_argument++;
          length_before_argument++;
        }
        argument[index++] = line[i];
      }
      argument[index] = '\0';
      char full_path_cur_dir[1024];
      getcwd(full_path_cur_dir, sizeof(full_path_cur_dir));

      DIR *directory;
      struct dirent *entry;
      directory = opendir(full_path_cur_dir);
      if (directory == NULL)
      {
        perror("Error opening directory\n");
        return 1;
      }
      char completed_file[1024] = "";
      char matches[MAX_MATCHES][1024];
      size_t match_count = 0;
      size_t cur_highest_counter = 0;
      while ((entry = readdir(directory)) != NULL)
      {
        char *file_name = entry->d_name;
        size_t matching_chars = 0;
        if (strlen(file_name) >= strlen(argument))
        {
          for (size_t i = 0; i < strlen(argument); i++)
          {
            if (file_name[i] == argument[i])
            {
              matching_chars++;
            }
            else // there was a char that didnt match so not a candidate
            {
              matching_chars = 0;
              break;
            }
          }
          if (matching_chars != 0)
          {
            if (cur_highest_counter == matching_chars)
            {
              snprintf(matches[match_count++], strlen(file_name) + 1, "%s", file_name);
              completed_file[0] = '\0';
            }
            else if (cur_highest_counter < matching_chars)
            {
              snprintf(completed_file, strlen(file_name) + 1, "%s", file_name);
              for (size_t i = 0; i < match_count; i++)
              {
                matches[i][0] = '\0';
              }
              snprintf(matches[0], strlen(file_name) + 1, "%s", file_name);
              match_count = 1;
              cur_highest_counter = matching_chars;
            }
          }
        }
      }
      if (closedir(directory) == -1)
      {
        perror("Error closing directory\n");
        return 1;
      }
      if (completed_file[0] == '\0')
      {
        if (pressed_tab == false)
        {
          write(STDOUT_FILENO, "\x07", sizeof(char));
          pressed_tab = true;
          continue;
        }
        else
        {
          if (match_count != 0)
          {
            printf("\n");
            for (size_t i = 0; i < match_count; i++)
            {
              printf("%s  ", matches[i]);
            }
            printf("\n");
            write(STDOUT_FILENO, "\r\033[K$ ", 6);
            write(STDOUT_FILENO, line, sizeof(char) * strlen(line));
          }
          continue;
        }
      }
      index = 0;
      *length = length_before_argument + strlen(completed_file);
      size_t i = 0;
      for (i = length_before_argument; i < *length; i++)
      {
        line[i] = completed_file[index++];
      }
      line[i] = '\0';
      write(STDOUT_FILENO, "\r\033[K$ ", 6);
      write(STDOUT_FILENO, line, sizeof(char) * strlen(line));
    }
    else
    {
      line[(*length)++] = c;
      write(STDOUT_FILENO, &c, sizeof(char));
    }
  }
  return 0;
}

void setTerminalMode(struct termios *old_attr)
{
  // set terminal into raw/non canonical mode
  tcgetattr(STDIN_FILENO, old_attr);
  struct termios new_attr = *old_attr;
  new_attr.c_lflag &= ~ICANON; // disable canonical mode so i can process byte by byte
  new_attr.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
}

int add_to_history(char *line_copy, char history_path[])
{
  add_history(line_copy); // put it into readline internally arrow up down works
  int fd = open(history_path, O_WRONLY | O_APPEND, 0644);
  if (fd == -1)
  {
    fprintf(stderr, "History couldnt be opened\n");
    return 1;
  }
  write(fd, line_copy, strlen(line_copy));
  write(fd, "\n", sizeof(char));
  return 0;
}

int handleDeclare(char output[][1024], DeclareVariable *new_variable, size_t *variable_count)
{
  if ('0' <= output[1][0] && '9' >= output[1][0])
  {
    printf("declare: `%s': not a valid identifer\n", output[1]);
  }
  char *line = strchr(output[1], '='); // returns a char* to the first ocurrence of =
  if (line != NULL)
  {
    if (*variable_count >= 1024)
      return 1;
    *line = '\0';
    snprintf(new_variable[*variable_count].type, sizeof(new_variable[*variable_count].type), "%s", output[1]);
    snprintf(new_variable[*variable_count].name, sizeof(new_variable[*variable_count].name), "%s", line + 1); // goes to first char of word and reads until \0
    (*variable_count)++;
  }
  else if (strcmp(output[1], "-p") == 0)
  {
    for (size_t i = 0; i < *variable_count; i++)
    {
      if (strcmp(new_variable[i].type, output[2]) == 0)
      {
        printf("declare -- %s=%s\n", new_variable[i].type, new_variable[i].name);
        return 0;
      }
    }
    printf("declare: %s: not found\n", output[2]);
  }
  else
  {
    printf("declare: variable: not found");
  }
  return 0;
}

int variable_expansion(char output[][1024], size_t* amount_tokens, DeclareVariable *variables, size_t variable_count)
{
  for (size_t i = 1; i < *amount_tokens; i++)
  {
    if (output[i][0] == '$')
    {
      if (output[i][1] == '{')
      {
        // brace expansion
        char *closing_brace = strchr(output[i], '}');
        if (closing_brace == NULL)
        {
          fprintf(stderr, "No valid input\n");
          return 1;
        }
        *closing_brace = '\0';
        char after_closing_bracket[1024] = "";
        if (*(closing_brace + 1) != '\0')
        {
          snprintf(after_closing_bracket, sizeof(after_closing_bracket), "%s", closing_brace + 1);
        }
        char *variable_name = output[i] + 2;
        for (size_t j = 0; j < variable_count; j++)
        {
          if (strcmp(variable_name, variables[j].type) == 0)
          {
            snprintf(output[i], sizeof(output[i]), "%s%s", variables[j].name, after_closing_bracket);
            break;
          }
          else
          {
            snprintf(output[i], sizeof(output[i]), "%s", after_closing_bracket);
            break;
          }
        }
      }
      else
      {
        for (size_t j = 0; j < variable_count; j++)
        {
          if (strcmp(output[i] + 1, variables[j].type) == 0)
            snprintf(output[i], sizeof(output[i]), "%s", variables[j].name);
          break;
        }
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[])
{
  char history_path[1024];
  int ret_value = loadHistory(history_path);
  if (ret_value != 0)
    return 1;
  DeclareVariable variable[1024];
  size_t variable_count = 0;
  struct termios old_attr;
  using_history();
  setTerminalMode(&old_attr);
  while (1)
  {
    setbuf(stdout, NULL);

    printf("$ ");
    size_t length = 0;
    size_t capacity = 100;
    char *line = malloc(sizeof(char) * capacity);
    if (line == NULL)
      return 1;
    int ret_value = readUserInput(line, &length, capacity);
    if (ret_value != 0)
    {
      tcsetattr(STDIN_FILENO, TCSANOW, &old_attr);
      return 1;
    }

    line[length] = '\0';
    printf("\n");
    char *line_copy = strdup(line);
    // addToHistory(input_history, line_copy, &counting_input);

    char output[10][1024];
    size_t amount_tokens = 0;
    handleQuotes(line, output, &amount_tokens);
    output[amount_tokens][0] = '\0';
    char *command = output[0];
    int saved_fd = -1;
    int target_fd = -1;
    bool redirected = false;
    ret_value = variable_expansion(output, &amount_tokens, variable, variable_count);
    if (ret_value != 0)
      return 1;
    ret_value = redirect_output(output, &amount_tokens, &target_fd, &redirected, &saved_fd);
    if (ret_value != 0)
      return 1;
    if (strcmp(command, "") != 0)
      add_to_history(line_copy, history_path);

    if (strcmp(command, "exit") == 0)
    {
      free(line_copy);
      free(line);
      break;
    }
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
    else if (strcmp(command, "declare") == 0)
      handleDeclare(output, variable, &variable_count);
    else if (strcmp(command, "") == 0)
    {
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
