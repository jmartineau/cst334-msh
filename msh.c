// Title: msh.c
// Abstract: This program further expands msh (a Unix shell) functionality
//	     by adding redirection.
// Author: Joseph Martineau
// Class: CST334 - Operating Systems
// Date: 09/26/2017

#include <fcntl.h>	// For open()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>	// Unix library

// Max length input string
#define MAXSTR 200

// Max number of arguments
#define MAXARGS 100

// Max length file path
#define MAXFILEPATH 260

// Function prototypes
int is_empty(const char *s);
void print_date();
int process_input(char *user_input);

int main(int argc, char **argv) 
{
	// Command-line arguments were passed in
	if (argc != 1)
	{
		// Initialize and open file
		FILE *file;
		char* file_name = argv[1];
		file = fopen(file_name, "r");
		
		// File exists
		if (file)
		{
			// File string
			char user_input[MAXSTR+3];
			
			// Read file information as user input
			if (fgets(user_input, MAXSTR+3, file) != NULL)
			{
				 // Successful read
			}
			// If fgets() returns null, exit (Ctrl + D detected)
			else
			{
				printf("\n");
				exit(0);
			}
			
			// Terminates string at first carriage return or new line
			user_input[strcspn(user_input, "\r\n")] = 0;
			
			process_input(user_input);
		}
		// File doesn't exist, print error
		else
		{
			printf("File does not exist or cannot be opened\n");
		}

		return 0;
	}
	
	// Keep running the shell until the user exits or presses Ctrl + D
	while(!feof(stdin))
	{
		// User input string
		char user_input[MAXSTR+3];
		
		// Prompt for input
		printf("msh> ");
	   
		// Read input string; at most MAXSTR+1 chars accepted
		// Note: This is tricky. If we accept only MAXSTR chars,
		// we can't see if user entered more chars and they are
		// being dropped by fgets.
		char* fgets_return = fgets(user_input, MAXSTR+3, stdin);
		
		// Remove carriage return from input
		user_input[strlen(user_input) - 1] = 0;
		
		// If fgets() returns null, exit (Ctrl + D detected)
		if (fgets_return == NULL)
		{
			printf("\n");
			exit(0);
		}
		
		process_input(user_input);
	}
   
    return 0;
}

// Checks if a string contains only whitespace
// Returns 1 if the string is empty, 0 otherwise
// From: https://stackoverflow.com/a/3981593
int is_empty(const char *s) 
{
  while (*s != '\0') 
  {
    if (!isspace((unsigned char)*s))
      return 0;
    s++;
  }
  return 1;
}

// Prints out the current date as: <month> <day-of-month>, <year>
// Modified from: http://bit.ly/2y0DXPY
void print_date()
{
	time_t timer;		// Value holding time since epoch
	struct tm* tm_info;	// Contains calendar date/time components
	
	// Holds string values of day/month/year
	char day[3];
	char month[11];
	char year[5];
	
	time(&timer);				// Returns system time since epoch
	tm_info = localtime(&timer);		// Fills tm_info with local time info
	
	// Formats time from tm_info as a string
	strftime(day, 3, "%d", tm_info);
	strftime(month, 11, "%B", tm_info);
	strftime(year, 5, "%Y", tm_info);
	
	printf("%s %s, %s\n", month, day, year);
}

// Handle user input from stdin or file
int process_input(char *user_input)
{
	// Handle empty string case (causes segfault if reaches tokenization)
	// Empty is defined as a string that contains only whitespace
	if (is_empty(user_input))
	{
		return 1;
	}
	
	// Check input length; n does not include final carriage return
	int n = strlen(user_input) - 1;  
	if (n > MAXSTR) 
	{
		printf("Input cannot be more than %d characters\n", MAXSTR);
		exit(1);
	}
	
	// Deal with whitespace before and after commands, also set up tokenization
	char *token;					// Holds individual string token
	char *search = " \t";				// Delimiter for "strtok" function
	token = strtok(user_input, search);		// Initialize token
   
	// If the user enters 'exit' (case sensitive), end the program
	if (strcmp(token, "exit") == 0)
	{
		exit(0);
	}
	// If the user enters 'help', print out information regarding program use
	else if (strcmp(token, "help") == 0)
	{
		printf("enter Linux commands, or ‘exit’ to exit\n");
	}
	// If the user enters 'today', print out <month> <day-of-month>, <year>
	else if (strcmp(token, "today") == 0)
	{
		print_date();
	}
	// If the user enters cd <arg>, change to <arg> directory
	else if (strcmp(token, "cd") == 0)
	{
		// Get next token after "cd" (anything else is ignored)
		token = strtok(NULL, search);
		
		// Holds return code of chdir() (0 = success, -1 = fail)
		int chdir_rc = 0;
		
		// Return user to home directory
		if (token == '\0' || strcmp(token, "~") == 0)
		{
			token = getenv("HOME");	// Get home directory path
			chdir_rc = chdir(token);
		}
		// Change to <arg> directory
		else
		{
			chdir_rc = chdir(token);
		}
		
		// chdir() failed, print error message
		if (chdir_rc == -1)
		{
			printf("-msh: cd: %s: No such file or directory\n", token);
		}
		
	}
	else	// Any other valid command gets executed
	{	
		// Code modified from OSTEP Figure 5.3
		int rc = fork();
		if (rc < 0) 
		{
			// Fork failed; exit
			fprintf(stderr, "fork failed\n");
			exit(1);
		} 
		// Child (new process)
		else if (rc == 0) 
		{
			// Handles extraneous printing when EOF is sent (Ctrl + D)
			if(feof(stdin))
			{
				printf("\n");
				exit(0);
			}
			
			// Loop until entire string is tokenized
			char *myargs[MAXARGS];	// Holds Linux command composed of string tokens
			int i = 0;		// Counter for myargs
			int in, out;
			while (token != NULL)
			{
				myargs[i] = strdup(token);
				char *token_to_compare = strdup(token);
				token = strtok(NULL, search);	// Pass NULL to keep searching same string
				i++;
				
				// Check for output redirection (">")
				// Help from: http://www.cs.loyola.edu/~jglenn/702/S2005/Examples/dup2.html
				if (strcmp(token_to_compare, ">") == 0 && token != NULL)
				{
					char *file_to_open = strdup(token);
					// Open output file
					out = open(file_to_open, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR 
						   | S_IRGRP | S_IWGRP | S_IWUSR);
					dup2(out, 1);		// Redirect file descriptor 1 (stdout) to file
					close(out);		// Close file descriptor after redirection
					
					// Handle redirection token and outfile so execvp does not run it
					token = strtok(NULL, search);
					i--;
				}
				
				// Check for input redirection ("<")
				if (strcmp(token_to_compare, "<") == 0 && token != NULL)
				{
					char *file_to_open = strdup(token);
					in = open(file_to_open, O_RDONLY);	// Open input file
					dup2(in, 0);				// Redirect file descriptor 0 (stdin) to file
					close(in);				// Close file descriptor after redirection
					
					// Handle redirection token and infile so execvp does not run it
					token = strtok(NULL, search);
					i--;
				}
			}
			
			myargs[i] = NULL;		// Marks end of array
			execvp(myargs[0], myargs); 	// Runs Linux command
			
			// If we get to this line, execvp() failed
			printf("%s: command not found\n", user_input);
			exit(1);	// Kill this process
		}
		// Parent goes down this path (original process)
		else 
		{
			wait(NULL);	// Return control to user once command is finished
		}
	}
}
