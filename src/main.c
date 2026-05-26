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
    printf("%s: command not found\n", buffer);
    printf("$ exit");
    break;
  }
  return 0;
}
