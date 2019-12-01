// Aniket Sanghi
// 170110

/*	Explanation of the Logic

	First for the Root directory 

	Func printDirectorySizes() do read the root directory and for each immediate sub-directory it creates a process via fork()
	then each of this forked child process recursively compute their sizes via Func printImmediateDirectorySizes()
	and print them to STDOUT and also write the size in the PIPE

	Now the parent process (i.e. of root) will read the sizes of all immediate sub-directory via PIPE 
	and sum them to find the size of the root directory and print it to STDOUT

	Main Functions

	-	printDirectorySizes() will create N processes for each immediate child-sub-directory and then read their sizes though pipe
							  and then finally print the root directory size to STDOUT
	-	printImmediateDirectorySizes() will recursively sum up the sizes of all the sub-childs and then print the net size of 
									   immediate sub-directory of root (IDENTIFIED VIA FLAG == 1) to STDOUT and PIPE's write port

	Helper Functions

	-	isDirectory() to verify the path to be of directory
	-	getFileSize   uses stat to output the size of the file
	-	intToString	  to convert a long long int value to string value
	-	extractRoot	  to find the root directory name from its path

*/
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

// To Check if the path is of a directory
int isDirectory(const char *path) {

	struct stat stats;
	if(stat(path, &stats) != 0) return 0;
	return S_ISDIR(stats.st_mode);
}

// To get the size of the file by using stat
int getFileSize(const char *path) {

	struct stat stats;
	if(stat(path, &stats) != 0) return 0;
	return stats.st_size;
}

// Function to convert integer to string
void intToString(long long int num, char *ans) {

	char string[50];
	int i = 0;

	// Store digits from ones to MES
	while(num) {
		string[i] = (num%10 + '0');
		num = num/10;
		i++;
	}
	string[i] = '\0';

	// Reverse the string
	int n = strlen(string);

	for(int i = 0; i<n; ++i) ans[i] = string[n-1-i];
	ans[n] = '\0';

	return;
}

long long int printImmediateDirectorySizes(int pipe_write, char *curr_directory, int flag) {

	// To store directory size
	long long int size = 0;

	// Opening directory
	DIR *dir = opendir(".");
	struct dirent *data;

	if(dir != NULL) {	/* If the directory is successfully opened */
		
		// Read file and directory one by one from the directory
		data = readdir(dir);
		while(data != NULL) {

			// To avoid infinite loop
			if(strcmp(data->d_name, ".") != 0 && strcmp(data->d_name, "..") != 0 ) {
				
				// If this is Directory
				if(isDirectory(data->d_name)) {

					// Recurse into the directory and sum up the sizes
					if(!chdir(data->d_name)) {
						size += (long long int)printImmediateDirectorySizes(0,data->d_name,0);
						chdir("..");		// Coming back to *curr_directory*
					}

				}
				else {

					size += (long long int)getFileSize(data->d_name);
				}
			}
			data = readdir(dir);
		}
		closedir(dir);
	}

	// If these are immediate sub-directories of root then
	if(flag == 1) {

		// Print to STDOUT and write to pipe
		printf("%s %lld\n", curr_directory, size);

		char string[50];
		
		// Converting int to string
		intToString(size, string);

		write(pipe_write, string, strlen(string));

		// Close pipe's write end and exit
		if(close(pipe_write) != 0) {
			perror("close");
			exit(-1);
		}
		exit(0);	
	}

	return size;
	
}

// The recursive function to print all directory sizes
void printDirectorySizes(char *path) {

	// To store the size of the current directory
	long long int size = 0;

	// Open your current directory and make pointer to store read data from directory
	DIR *dir = opendir(".");
	struct dirent *data;

	if(dir != NULL) {		// If the directory is successfully opened

		// Read one-by-one data from directory
		data = readdir(dir);
		while(data != NULL) {	// While there are objects in directory

			// To avoid going back again and getting stuck in infinite loop
			if(strcmp(data->d_name, ".") != 0 && strcmp(data->d_name, "..") != 0 ) {

				// If the object is a directory
				if(isDirectory(data->d_name)) {
					
					// Creating a pipe to communicate between this and the new process
					int fd[2];
					if(pipe(fd) < 0) {
						perror("pipe");
						exit(-1);
					}

					// Forking this process to create new process
					pid_t pid;
					
					pid = fork();
					if(pid < 0) {
						perror("fork");
						exit(-1);
					}

					// Now the child process will find the value for the new directory
					if(pid == 0) {		// Child

						// Close the read end of pipe
						if(close(fd[0]) != 0) {
							perror("close read end of pipe");
						}

						// Enter in the directory
						if(chdir(data->d_name) != 0) {
							perror("can't enter into directory");
							exit(-1);
						}

						// Now find the size of this directory (and the ones inside it)
						// And send write its size in the pipe
						printImmediateDirectorySizes(fd[1], data->d_name, 1);

					}

					wait(NULL);			// Waiting for the child processes to finish

					// The above process must have written the size of that immediate subdirectory in the pipe
					// Retriving that from pipe and adding it to our root directory size
					
					char buff[50];	// Considering 64 bit integer value, it can be max 19 digits

					// Close the write end of pipe. So that read can find EOF
					if(close(fd[1]) < 0) {
						perror("close");
						exit(-1);
					}

					// reading from pipe
					int bytes_read = read(fd[0], buff, 50);
					buff[bytes_read] = '\0';	// Mark end of string to avoid errors

					// Add it to the size of the root directory
					size = size + (long long int)atoi(buff);

				}

				// If it is not a directory then just add its size
				else {

					size += (long long int)getFileSize(data->d_name);
				}
			}

			data = readdir(dir);
		}

		closedir(dir);
	}

	printf("%s %lld\n", path, size);
}

// To extract root directory name from the path
void extractRoot(char *path, char *root) {

	int n = strlen(path);
	int i = 0, j = 0;

	while(i < n) {

		if(path[i] == '/') {
			root[j] = '\0';
			j = -1;
		}
		else root[j] = path[i];

		j++; i++;
	}
	if(j != 0) root[j] = '\0';
	return;
}

int main(int argc, char *argv[]) {

	if(argc < 2) {
		perror("Please Enter valid number of arguments");
		exit(-1);
	}

	// If the address given is not of a directory
	if(!isDirectory(argv[1])) {
		perror("Not a directory");
		exit(-1);
	}

	// Extracting name of root from given address
	char root[PATH_MAX];
	extractRoot(argv[1], root);

	// Enter in the directory and recursively iterate over it
	if(!chdir(argv[1]))
		printDirectorySizes(root);
	
	return 0;
}


