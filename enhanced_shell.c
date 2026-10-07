#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_TOK 50
#define MAX_CMD 50

int main() {

  while (1) {
    char user_input[50];
    char* argv[MAX_TOK]; // pointer to array of char ptrs
    char* tok;
    int tokCount = 0;
    int cmdCount = 1;
    int cmdStart[MAX_CMD]; // store index of start of each command
    
    // pointer to array of each redirection file
    char* stdInFiles[MAX_CMD];
    char* stdOutFiles[MAX_CMD];

    // prompt and store user input, exit on error
    printf("Enter a command: ");
    if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
      printf("\nEOF or an error has occured reading from stdin. Exiting...\n");
      break;
    }
    
    // parse
    cmdStart[0] = 0;
    tok = strtok(user_input, " \n");
    while (tok != NULL && tokCount < MAX_TOK - 1) { // leave room for NULL (just in case)
      if (strcmp(tok, "|") == 0) {

        // append NULL to current argv array when "|" seen
        argv[tokCount] = NULL;
        tokCount++;
        
        // store starting index of command
        cmdStart[cmdCount] = tokCount;
        cmdCount++;
      } else if (strcmp(tok, ">") == 0) { // stdout func.
        tok = strtok(NULL, " \n"); // end before filename
        if (tok == NULL) {
          perror("No file name input after '>'.\n");
          exit(3);
        }
        stdOutFiles[cmdCount - 1] = tok;
      } else if (strcmp(tok, "<") == 0) { // stdin func.
        tok = strtok(NULL, " \n");
        if (tok == NULL) {
          perror("No file name input after '<'. \n");
          exit(4);
        }
        stdInFiles[cmdCount - 1] = tok;
      } else {  
        argv[tokCount] = tok;
        tokCount++;
      }
      tok = strtok(NULL, " \n"); // continue after each command "end" within the user_input string
    }
    
    // notify and continue if no command entered
    if (tokCount == 0) {
      printf("No command entered.\n");
      continue;
    }
    
    // check first arg. for "quit" flag
    if (strcmp(argv[0], "quit") == 0) {
      printf("Quitting...\n");
      exit(93);
    }

    // append NULL to end of argv
    argv[tokCount] = NULL;
    
    // create pipes for each command (besides last)
    int fd[MAX_CMD - 1][2];
    for (int p = 0; p < cmdCount - 1; p++) {
      if (pipe(fd[p]) < 0) {
        perror("Pipe failed.");
        exit(1);
      }
    } 
    
    // each command, a child process is created
    for (int i = 0; i < cmdCount; i++) {
      pid_t child = fork();
      
      if (child < 0) {
        perror("Fork failed.");
        exit(2);
      }

      if (child == 0) {
         
        // read from previous pipe, not initial command (which reads from stdin)
        if (i > 0) {
          dup2(fd[i-1][0], 0);
        }

        // write into next pipe, not last command (which doesn't need pipe)
        if (i < cmdCount - 1) {
          dup2(fd[i][1], 1);
        }

        // close ends of all pipes
        for (int e = 0; e < cmdCount - 1; e++) {
          close(fd[e][0]);
          close(fd[e][1]);
        }
       
        // stdout func. 
        if (stdOutFiles[i] != NULL) {
          int fd_rec = open(stdOutFiles[i], O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
          dup2(fd_rec, 1);
          close(fd_rec);
        }

        // stdin func.
        if (stdInFiles[i] != NULL) {
          int fd_in = open(stdInFiles[i], O_RDONLY, 0);
          dup2(fd_in, 0);
          close(fd_in);
        }
        
        // run command w/ given args, else error
        if (execvp(argv[cmdStart[i]], &argv[cmdStart[i]])) {
          perror("Unknown command, exec failed.");
          exit(2);
        }
        exit(100);
        }
      }
        
      // close pipes in parent.
      for (int x = 0; x < cmdCount - 1; x++) {
        close(fd[x][0]);
        close(fd[x][1]);
      }

      // wait for all child process to complete
      for (int i = 0; i < cmdCount; i++) {
        int status;
        wait(&status);
      }
    }
  return 0;
}
