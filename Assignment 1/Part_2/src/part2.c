// Aniket Sanghi
// 170110

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

void Operation1(char *, char *);
void Operation2(int, char **);

int main(int argc, char *argv[]) {
	if(argc < 4) {
		perror("Please enter valid number of arguments\n");
		exit(-1);
	}

	// If it is the operation of first type (@)
	if(argv[1][0] == '@') {
		Operation1(argv[2], argv[3]);
	}

	// If it is the operation of second type ($)
	else if(argv[1][0] == '$') {


		// Check for the number of arguments
		if(argc < 6) {
			perror("Please enter valid number of arguments for $\n");
			exit(-1);
		}
		Operation2(argc, argv);
	}

	return 0;
}

void Operation1(char *string, char *path) {

	int pid, fd[2];

	// Creating a pipe and checking for errors
	if (pipe(fd) < 0) {
		perror("pipe fault\n");
		return;
	}

	// Fork the process
	pid = fork();
	if(pid < 0) {
		perror("fork\n");
		exit(-1);
	}

	if(pid == 0) {			// Child Process

		// Closing STDOUT and assign that to fd[1]
		if(dup2(fd[1], 1) < 0) {
			perror("dup2 error\n");
			exit(-1);
		}

		// Closing fd[0] (read end of pipe) [NOT NECESSARY THOUGH]
		if(close(fd[0]) < 0) {
			perror("close error\n");
		}

		char *arg[] = {"grep","-rF",string,path,NULL};
		// Run your first grep process whose output will go in pipe
		if (execvp("grep", arg) != 0) {
			perror("execl\n");
			exit(-1);
		}
	}
	else {					// Parent process

		// Wait for the child process to end
		// wait(NULL);

		// Commented this wait because in case of long inputs
		// The pipe was not allowing grep to write because of 
		// Its memeory limit

		// Close fd[1] i.e write end of pipe
		// Otherwise fd[0] will wait for EOF 
		// Which won't be fixed if write end is open
		if(close(fd[1]) < 0){
			perror("close fd[1] error\n");
			exit(-1);
		}

		// Close STDIN and assign that to fd[0]
		if(dup2(fd[0], 0) < 0) {
			perror("dup2 error\n");
		}

		

		char *arg[] = {"wc","-l",NULL};
		// Run your second wc -l process
		if (execvp("wc",arg) != 0) {
			perror("execvp\n");
			exit(-1);
		}
	}

	return;
}

// Function to do Operation 2 with $
void Operation2(int argc, char *argv[]) {

	char *string = argv[2];
	char *path = argv[3];
	char *command = argv[5];
	char *arguments[argc-4];

	// Filling the arguments of the bash command
	int i = 0; 
	while(i<argc-5) {
		arguments[i] = argv[5+i];
		i++;
	}
	arguments[i] = NULL;

	int pid, fd[2], fd2[2];

	// Creating a pipe and checking for errors
	if (pipe(fd) < 0) {
		perror("pipe fault\n");
		return;
	}

	pid = fork();
	if(pid < 0) {
		perror("fork fault!!");
		exit(-1);
	}

	if(pid == 0) {		// Child Process

		// Closing STDOUT so as to write to pipe
		if(dup2(fd[1], 1) < 0) {
			perror("dup2 error:");
			exit(-1);
		}

		// Closing fd[0] (read end of pipe) [NOT NECESSARY THOUGH]
		if(close(fd[0]) < 0) {
			perror("close error\n");
		}

		// Run your first grep process whose output will go in pipe
		if (execl("/bin/grep", "grep", "-rF", string, path, NULL) != 0) {
			perror("execl\n");
			exit(-1);
		}
	}
	else {				// Parent Process

		// Wait for the child to finish
		// wait(NULL);

		// Creating a pipe and checking for errors
		if (pipe(fd2) < 0) {
			perror("pipe fault\n");
			return;
		}

		pid = fork();
		if(pid < 0) {
			perror("fork fault!!");
			exit(-1);
		}

		// Close fd[1] i.e write end of pipe
		// Otherwise fd[0] will wait for EOF 
		// Which won't be fixed if write end is open
		// Closing it for both the processes! As they point to same!
		if(close(fd[1]) < 0){
			perror("close fd[1] error\n");
			exit(-1);
		}

		if(pid == 0) {		// New Child process

			// Opening file
			int file = open(argv[4], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH );
			if(file < 0) {
				perror("Couldn't write to file");
				exit(-1);
			}

			// Closing STDOUT so as to write to pipe
			if(dup2(fd2[1], 1) < 0) {
				perror("dup2 error:");
				exit(-1);
			}


			// Close STDIN and assign that to fd[0]
			if(dup2(fd[0], 0) < 0) {
				perror("dup2 error\n");
			}

			// Closing fd2[0] (read end of pipe) [NOT NECESSARY THOUGH]
			if(close(fd2[0]) < 0) {
				perror("close error\n");
			}

			int BUFF_SIZE = 1024;
			char buff[BUFF_SIZE + 1];
			int bytes_read = 0;


			// Reading from STDIN and writing to STDOUT and FILE
			while((bytes_read = read(0, buff, BUFF_SIZE)) > 0) {
				if(write(file, buff, bytes_read) != bytes_read) {
					perror("Writing to file left imcomplete");
					exit(-1);
				}

				if(write(1, buff, bytes_read) != bytes_read) {
					perror("Writing to stdout left imcomplete");
					exit(-1);
				}
				
			}


			close(file);
			exit(0);

		}
		else {		// Parent

			// Wait for the child to finish
			// wait(NULL);

			// Close STDIN and assign that to fd[0]
			if(dup2(fd2[0], 0) < 0) {
				perror("dup2 error\n");
			}

			// Close fd2[1] i.e write end of pipe
			// Otherwise fd2[0] will wait for EOF 
			// Which won't be fixed if write end is open
			if(close(fd2[1]) < 0){
				perror("close fd2[1] error\n");
				exit(-1);
			}

			// Run the command on the output
			if(execvp(command, arguments) != 0) {
				perror("execvp error\n");
				exit(-1);
			}
		}

	}

	return;
}

