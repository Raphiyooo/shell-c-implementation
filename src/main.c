#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  while (1)
  {
    setbuf(stdout, NULL);

    printf("$ ");
    char buffer[1024];
    fgets(buffer, sizeof(buffer), stdin);

    buffer[strlen(buffer) - 1] = '\0';

    char* tokens = strtok(buffer, " ");
    char* args = strtok(NULL, "");
    if (tokens == NULL)
      continue;

    if (strcmp(tokens, "exit") == 0)
      break;
    else if (strcmp(tokens, "echo") == 0)
    {
      printf("%s\n", args);
    }
    else if (strcmp(tokens, "type") == 0)
    {
      char *name = getenv("PATH");
      if (name != NULL)
      {
        // error checking
      }
      if (!strcmp(args, "exit") || !strcmp(args, "echo") || !strcmp(args, "type"))
        printf("%s is a shell builtin\n", args);
      
      else if (name != NULL)
      {
        char* tokens = strtok(name, ':');
        while (tokens != NULL)
        {
          if (access(tokens, X_OK) == 0)
          {
            printf("%s is %s\n", args, tokens);
          }
          strtok(NULL, ':');
        }
      }
      else
        printf("%s not found\n", args);
    }
    else
    {
      printf("%s: command not found\n", buffer);
    }
  }
  return 0;
}
