#include <unistd.h>

#include <stdbool.h>

#include <sys/wait.h>

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <fcntl.h>

#include <errno.h>

#define BUFSIZE 64 // Size of a single token
#define TOKEN_DELIM " \t\r\n\a" // Token Delimiters

static char * path = "/bin";

/* The breakline function takes a command line as input and splits it into tokens
 * delimited by the standard delimiters */
char ** BreakLine(char * string) {
  int bufsize = BUFSIZE, k = 0;
  char ** tokens = malloc(bufsize * sizeof(char * ));
  char * token;

  if (!tokens) {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(EXIT_FAILURE);
  }

  token = strtok(string, TOKEN_DELIM); // Get the first token
  while (token != NULL) {
    tokens[k] = token;
    k++;

    if (k >= bufsize) { // If number of tokens are more then reallocate
      bufsize += BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char * ));
      if (!tokens) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOKEN_DELIM);
  }
  tokens[k] = NULL; // indicate token end
  if (k > 0) {
    char * tmp = malloc(1024 * sizeof(char * ));
    strcpy(tmp, tokens[k - 1]);
    char * pk = strtok(tmp, ">");
    char * pk_1 = strtok(NULL, ">");
    if (pk_1 != NULL) {
      tokens[k - 1] = pk;
      tokens[k] = ">";
      tokens[k + 1] = pk_1;
    }
  }
  return tokens;
}

/* The StartProcess function initiates a foreground or background process using fork() and
 * execv() function calls. */
pid_t StartProcess(char ** args, int k) {
  pid_t pid;

  char * pch;
  char * delim = ":";
  char * tmp = malloc(1024 * sizeof(char * ));
  strcpy(tmp, path);
  pch = strtok(tmp, delim);
  bool found = false;
  while (pch != NULL) {
    char * realpath = malloc(1024 * sizeof(char * ));
    strcpy(realpath, pch);
    strcat(realpath, "/");
    strcat(realpath, args[0]);
    if (access(realpath, X_OK) == 0) {
      found = true;
      if (strcmp(args[0], "ls") != 0) {
        args[0] = realpath;
        break;
      }
    }
    pch = strtok(NULL, delim);
  }

  if (k > 0)
    args[k] = NULL;
  pid = fork();
  if (pid == 0) // fork success. child initiated
  {
    if (!found) {
      char error_message[30] = "An error has occurred\n";
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(EXIT_FAILURE);
    }

    if (strcmp(args[0], "ls") == 0) {
      execvp(args[0], args);
    } else {
      execv(args[0], args);
    }
    //char error_message[30] = "An error has occurred\n";
    //write(STDERR_FILENO, error_message, strlen(error_message));
    if (k == 0)
      exit(EXIT_FAILURE);
  }
  return pid;
}

/* IORedirect function redirects Standard Input or Standard Output depending on value of
 * parameter ioMode */
pid_t IORedirect(char ** args, int k, int ioMode) {
  char * pch;
  char * delim = ":";
  char * tmp = malloc(1024 * sizeof(char * ));
  strcpy(tmp, path);
  pch = strtok(tmp, delim);

  pid_t pid;
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int fd;

  pid = fork();
  if (pid == 0) {
    // Child process
    if (ioMode == 0) // Input mode
      fd = open(args[k + 1], O_RDONLY, mode);
    else { // Output mode
      fd = open(args[k + 1], O_RDWR | O_CREAT | O_TRUNC, mode);
    }
    if (fd < 0) {
      char error_message[30] = "An error has occurred\n";
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(0);

    } else {
      dup2(fd, ioMode); // Redirect input or output according to ioMode
      close(fd); // Close the corresponding pointer so child process can use it

      args[k] = NULL;
      args[k + 1] = NULL;
      if (k == 0) {
        //args[0] = "echo";
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
      }
      while (pch != NULL) {
        char * realpath = malloc(1024 * sizeof(char * ));
        strcpy(realpath, pch);
        strcat(realpath, "/");
        strcat(realpath, args[0]);
        if (access(realpath, X_OK) == 0) {
          args[0] = realpath;
          break;
        }
        pch = strtok(NULL, delim);
      }
      if (execv(args[0], args) == -1) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(0);
      }
    }
  } else if (pid < 0) { // Error forking process
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(0);
  }
  return pid;
}
void RunBatch(char * filename) {

  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  char ** args;
  int k = 0;
  int option = 0;
  char * options[2] = {
    "<",
    ">"
  };
  fp = fopen(filename, "r");
  if (fp == NULL) {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(EXIT_FAILURE);
  }
  int process = 0;
  while ((read = getline( & line, & len, fp)) != -1) {

    char * tmp_line = malloc(1024 * sizeof(char * ));
    strcpy(tmp_line, line);

    char * cmd;
    pid_t * pids = malloc(1024 * sizeof(pid_t));
    int count = 0;
    while ((cmd = strtok_r(tmp_line, "&", & tmp_line))) {
      int found = 0;
      args = BreakLine(cmd); // Split command line into tokens
      if (args[0] == NULL) {
        continue;
      }
      process++;
      if (strcmp(args[0], "exit") == 0) { // Handle the exit command. Break from command loop
        if (args[1] != NULL) {
          char error_message[30] = "An error has occurred\n";
          write(STDERR_FILENO, error_message, strlen(error_message));
          // exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
      } else if (strcmp(args[0], "cd") == 0) { // Handle cd command.
        if (args[1] == NULL || args[2] != NULL) {
          char error_message[30] = "An error has occurred\n";
          write(STDERR_FILENO, error_message, strlen(error_message));
          //exit(EXIT_FAILURE);
        } else {
          if (chdir(args[1]) != 0) {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(EXIT_FAILURE);
          }
        }
      } else if (strcmp(args[0], "path") == 0) { // Handle path command.
        path = malloc(1024 * sizeof(char * ));
        strcpy(path, "");
        for (int i = 1; i < sizeof(args); i++) {
          if (args[i]) {
            strcat(path, ":");
            strcat(path, args[i]);
          }
        }
      } else {
        k = 0;
        while (args[k] != NULL) { // Check for any of the redirect or process operators <,<,|,&
          for (option = 0; option < 2; option++) {
            if (strcmp(args[k], options[option]) == 0)
              break;
          }
          if (option < 2) {
            found = 1;
            if (args[k + 1] == NULL || args[k + 2] != NULL) { // 1 argument is necessary
              char error_message[30] = "An error has occurred\n";
              write(STDERR_FILENO, error_message, strlen(error_message));
              exit(0);
            }
            pids[count++] = IORedirect(args, k, option);
            break;
          }

          k++;
        }
        if (found == 0) {
          pids[count++] = StartProcess(args, 0);
        }
      }
    }
    int status;
    for (; count > 0; count--) {
      do {
        waitpid(pids[count - 1], & status, WUNTRACED);
      }
      while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
  }
  if (process == 0) {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(EXIT_FAILURE);
  }
  fclose(fp);
  exit(EXIT_SUCCESS);
}
int main(int argc, char ** argv) {
  //path = getenv("PATH");
  path = "/bin";
  // path = "";
  if (argc > 1) { // batch mode
    RunBatch(argv[1]);
    return 0;
  }

  char * cmdtext = NULL;
  char ** args, * options[2] = {
    "<",
    ">"
  };
  int k = 0, option, found;

  do {
    size_t bsize = 0;
    found = 0;
    printf("\nwish>");
    getline( & cmdtext, & bsize, stdin);
    char * tmp_line = malloc(1024 * sizeof(char * ));
    strcpy(tmp_line, cmdtext);
    char * cmd;
    pid_t * pids = malloc(1024 * sizeof(pid_t));
    int count = 0;
    while ((cmd = strtok_r(tmp_line, "&", & tmp_line))) {
      args = BreakLine(cmd);

      if (args[0] == NULL) {
        free(cmdtext);
        cmdtext = NULL;
        free(args);
        args = NULL;
        continue;
      }
      if (strcmp(args[0], "exit") == 0) { // Handle the exit command. Break from command loop
        if (args[1] != NULL) {
          char error_message[30] = "An error has occurred\n";
          write(STDERR_FILENO, error_message, strlen(error_message));
          exit(EXIT_FAILURE);

        }
        exit(EXIT_SUCCESS);
      } else if (strcmp(args[0], "cd") == 0) { // Handle cd command.
        if (args[1] == NULL || args[2] != NULL) {
          char error_message[30] = "An error has occurred\n";
          write(STDERR_FILENO, error_message, strlen(error_message));
          break;
        } else {
          if (chdir(args[1]) != 0) {
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            break;
          }
        }
      } else if (strcmp(args[0], "path") == 0) { // Handle path command.
        path = malloc(1024 * sizeof(char * ));
        strcpy(path, "");
        for (int i = 1; i < sizeof(args); i++) {
          if (args[i]) {
            strcat(path, ":");
            strcat(path, args[i]);
          }
        }
      } else {
        k = 1;
        while (args[k] != NULL) { // Check for any of the redirect or process operators <,<,|,&
          for (option = 0; option < 2; option++) {
            if (strcmp(args[k], options[option]) == 0)
              break;
          }
          if (option < 2) {
            found = 1;
            if (args[k + 1] == NULL) { // argument is necessary
              char error_message[30] = "An error has occurred\n";
              write(STDERR_FILENO, error_message, strlen(error_message));
              break;
            }

            pids[count++] = IORedirect(args, k, option);
            break;
          }

          k++;
        }
        if (found == 0) {
          pids[count++] = StartProcess(args, 0);
        }
      }
    }
    int status;
    for (; count > 0; count--) {
      do {
        waitpid(pids[count - 1], & status, WUNTRACED);
      }
      while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    if (cmdtext) free(cmdtext);
    if (args) free(args);
  }
  while (1);
}
