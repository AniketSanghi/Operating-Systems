// Aniket Sanghi
// 170110

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void searchInDirectory(char *, char *);
void searchInFile(char *, char *, char *);
void searchForString(char *, char *, char *);
int isDirectory(const char *);
int isRegularFile(const char *);
int KMP(char *, int, char *, int);
void LPS(char *, int, int *);
void searchInFileFirst(char *, char *);

int main(int argc, char *argv[]) {
	
	// Checking if valid number of arguments are entered
	if(argc < 3) {
		perror("Error: Please enter first argument as string and second as path to the directory\n");
		exit(-1);
	}
	// If the path is of file then no need for addresses
	if (isRegularFile(argv[2])) {
		searchInFileFirst(argv[2], argv[1]);
	}
	else 
		searchForString(argv[2], argv[1], argv[2]);

	return 0;
}

// Special case when the address of a file is given directly
void searchInFileFirst(char *path, char *string) {

	int file = open(path, O_RDONLY);
	if(file < 0) return;

	int str_len = strlen(string);
	
	// To store line
	char line[1000000];
	int line_len = 0;

	// Reading while end of file
	while(read(file, line+line_len, 1) > 0) {
		
		// If it is the end of line
		if(line[line_len] == '\n') {
			line[line_len] = '\0';
			
			if(KMP(line, line_len, string, str_len)) {
				printf("%s\n", line);
			}

			// Start reading new line
			line[0] = '\0';
			line_len = 0;
		}
		else line_len++;
	}

	close(file);

	return;
}

// Identify the path to be of directory or regular file
void searchForString(char *path, char *string, char *full_path) {

	// For Directory
	if(isDirectory(path)) {			

		if(!chdir(path)) {			/* If successful in changing directory */
			searchInDirectory(string, full_path);
			chdir("..");
		}
		return;
	}
	
	// For Regular File
	if(isRegularFile(path)) {

		searchInFile(path, string, full_path);
		return;
	}

	return;
}



// To Check if the path is of a directory
int isDirectory(const char *path) {

	struct stat stats;
	if(stat(path, &stats) != 0) return 0;
	return S_ISDIR(stats.st_mode);
}



// To Check if the path is of a regular file
int isRegularFile(const char *path) {

	struct stat stats;
	if(stat(path, &stats) != 0) return 0;
	return S_ISREG(stats.st_mode);
}

// To read directory recursively and call respective functions
void searchInDirectory(char *string, char *full_path) {
	
	DIR *dir = opendir(".");
	struct dirent *data;
	char new_path[PATH_MAX];
	if(dir != NULL) {	/* If the directory is successfully opened */
		
		// Read file and directory one by one from the directory
		data = readdir(dir);
		while(data != NULL) {

			// To avoid infinite loop
			if(strcmp(data->d_name, ".") != 0 && strcmp(data->d_name, "..") != 0 ) {
				
				// Update the full_address recursively
				new_path[0] = '\0';
				strcat(new_path, full_path);
				if(new_path[strlen(new_path)-1] != '/')
					strcat(new_path, "/");
				strcat(new_path, data->d_name);
				searchForString(data->d_name, string, new_path);
			}
			data = readdir(dir);
		}
		closedir(dir);
	}

	return;
}

// Search in file reading byte by byte via read and use KMP to compare
void searchInFile(char *path, char *string, char *full_path) {

	int file = open(path, O_RDONLY);
	if(file < 0) return;

	int str_len = strlen(string);
	
	// To store line
	char line[1000000];
	int line_len = 0;

	// Reading while end of file
	while(read(file, line+line_len, 1) > 0) {
		
		// If it is the end of line
		if(line[line_len] == '\n') {
			line[line_len] = '\0';
			
			if(KMP(line, line_len, string, str_len)) {
				printf("%s:%s\n", full_path, line);
			}

			// Start reading new line
			line[0] = '\0';
			line_len = 0;
		}
		else line_len++;
	}

	close(file);

	return;
}

// Knuth Morris Pratt algorithm for pattern matching 
// Time complexity O(n + m)
int KMP(char *line, int l_len, char *string, int s_len) {
	
	if(l_len < s_len || s_len == 0) return 0;
	int lps[s_len];

	LPS(string, s_len, lps);

	int j = 0;
	for(int i = 0; i<l_len; ++i) {

		if(line[i] == string[j]) j++;
		else {

			while(j && line[i] != string[j]) j = lps[j-1];

			if(line[i] == string[j]) j++;
		}

		if(j == s_len) return 1;
	}
	return 0;
}

// Finding the longest prefix suffix!! Very cool observation!
void LPS(char *string, int s_len, int *lps) {

	if(s_len == 0) return;
	lps[0] = 0;
	int j = 0;
	for(int i = 1; i<s_len; ++i) {

		if(string[i] == string[j]) {
			lps[i] = j+1;
			j++;
		}
		else {
			while(j && string[i] != string[j]) j = lps[j-1];

			if(string[i] == string[j]) {
				lps[i] = j+1;
				j++;
			}
			else lps[i] = 0;
		}
	}
}