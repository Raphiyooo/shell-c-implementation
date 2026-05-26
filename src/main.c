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

    if (strcmp(buffer, "exit") == 0)
      break;
    else if (strcmp(buffer, "echo") == 0)
    {
      char *echo_arguments = buffer + 4;
      printf("%s\n", echo_arguments);
    }

    printf("%s: command not found\n", buffer);
  }
  return 0;
}
