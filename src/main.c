#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (strcmp(buffer, "exit") == 0)
      break;
    else if (strncmp(buffer, "echo", 5) == 0)
    {
      tokens = strtok(NULL, " ");
      while (tokens != NULL)
      {
        printf("%s ", tokens);
        tokens = strtok(NULL, " ");
      }
      printf("\n");
    }
    else
    {
      printf("%s: command not found\n", buffer);
    }
  }
  return 0;
}
