	#define _GNU_SOURCE
	#include <stdio.h>
	#include "apue.h"   
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/stat.h>        
	#include <sys/types.h>        
	#include <sys/wait.h>
	#include <stdlib.h>
	#include <string.h>
	#include <signal.h>
	#include <fcntl.h>

	//markargv array for tokens in s which are delimited
	int makeargv(char *s, char *delimiters, char ***argvp);

	//prototypes for built-in functions
	int com_cd(char *args);
	int com_umask(char *args);
	int com_exit(char *args);

	//global variables
	static int fg = 1; //keeps track if in bg or fg
	static char *home = NULL;
	char file_permission[4];
	char *cur_dir_path;

	char *get_line_from_stdin(void){
	  char *line = NULL;
	  size_t bufsize = 0;
	  getline(&line, &bufsize, stdin);
	  return line;
	} // end of get_line_from_stdin

	//built-in functions
	char *functions_str[] = {
		"cd",
		"umask",
		"exit"
	};

	//built-in function ptrs
	int (*functions_internal[])(char *) = {
		&com_cd,
		&com_umask,
		&com_exit
	};

	/* reset_perm used to reset permissions to      *
	*  the original permissions, so umask can reset *
	*  and the com_umask function can work correctly*/
	void reset_perm(void){
		if (file_permission[0] != '\0') {
			mode_t i = strtol(file_permission, 0, 8);
			chmod(cur_dir_path, i);
		}
	} // end of reset_perm

	//com_cd built-in function to cd to directory
	int com_cd(char *args){
		char cwd_reset = 0;
  		if (args == NULL && chdir(home) == 0) cwd_reset = 1;
  		else if (args != NULL && chdir(args) == 0) cwd_reset = 0;
  		else err_sys(args);
  
  		if (cwd_reset) {
    		chdir(cur_dir_path);
    		if (getcwd(cur_dir_path, (size_t)1024) == NULL) err_sys("getcwd fail");
  		}
  		return 1;
	} // end of com_cd


	//com_umask built-in function to umask user specified condition
	int com_umask(char *args) {
		DIR *midir;
		struct dirent* info_archivo;
		struct stat file_stat;
		int usr = 0,grp = 0,oth = 0;

		if ((midir = opendir(".")) == NULL){
			err_sys("error: when opening opendir");
			//resets permissions for com_umask function
			reset_perm();
			exit(EXIT_FAILURE);
		}

		while ((info_archivo = readdir(midir)) != 0) {
			if (strcmp(info_archivo->d_name,"./")){
				if(!stat(info_archivo->d_name, &file_stat)){
					
					//checking directory permissions, and setting them
					(file_stat.st_mode & S_IRUSR) ? usr +=4 : 0;
					(file_stat.st_mode & S_IWUSR) ? usr +=2 : 0;
					(file_stat.st_mode & S_IXUSR) ? usr +=1 : 0;
					(file_stat.st_mode & S_IRGRP) ? grp +=4 : 0;
					(file_stat.st_mode & S_IWGRP) ? grp +=2 : 0;
					(file_stat.st_mode & S_IXGRP) ? grp +=1 : 0;
					(file_stat.st_mode & S_IROTH) ? oth +=4 : 0;
					(file_stat.st_mode & S_IWOTH) ? oth +=2 : 0;
					(file_stat.st_mode & S_IXOTH) ? oth +=1 : 0;
					
					//get current working directory to save it to cur_dir_path
					cur_dir_path = getcwd(cur_dir_path, (size_t)1024);
					break; //kick out of function once if is satisfied
				}
			}
		} // end of while

		closedir(midir);
		//new permissions requested
		char *newperm = args;
		
		if (newperm == '\0'){
			//print current permission
			printf("0%i%i%i\n", 7-usr, 7-grp, 7-oth);
			return 1;
		}
		else {
			//setting permissions to global variable
			file_permission[0] = usr + '0';
			file_permission[1] = grp + '0';
			file_permission[2] = oth + '0';
			file_permission[3] = '\0';
			char *mode = args;
			mode_t perm = strtol(mode,0,8);
			//set permission to user requested permissions
			if (chmod(cur_dir_path, perm) < 0){
				//error check
				err_sys("error: chmod doesn't have permissions");
				//resets permissions for com_umask function
				reset_perm();
				exit(EXIT_FAILURE);
			}
		}

		return 1; //end of function
	} //end of com_umask

	/* com_exit used to exit the shell if prompted*/
	int com_exit(char *args){
		//resets permissions for com_umask function
		reset_perm();
		exit(0);
	} // end of com_exit

	//Fork process and creating Parent and Child
	int program_launch(int my_std_in, int my_std_out, char **args){
		pid_t pid, wpid = 0;
		int check;

		//creating new process
		pid = fork();

		if (pid == 0) { //child
			// make stdin the stdout that was passed 
			if (my_std_in != 0) {
		      	dup2(my_std_in, 0);
		      	close(my_std_in);
	    	}
	    	// make stdout the stdin that was passed 
	    	if (my_std_out != 1) {
		      	dup2(my_std_out, 1);
		      	close(my_std_out);
	    	}

			if (execvp(args[0], args) == -1) {
				err_sys("error: execvp failed");
				//resets permissions for com_umask function
				reset_perm();
				exit(EXIT_FAILURE);
			}
		}
		else if (pid < 0) {
			//forking error
			err_sys("error: fork failed");
		}
		else {
			//parent
			do {
				wpid = waitpid(pid, &check, WUNTRACED);
			} while (!WIFEXITED(check) && !WIFSIGNALED(check));
		}
		//either returns a 0 if success, or a 1 if failed
		return wpid;
	} // end of program launch

	//Pipe/Redirect (Some code referenced from http://www.gnu.org and http://stackoverflow.com/questions/19191030/pipe-fork-and-exec-two-way-communication-between-parent-and-child-process)
	int release_children(int num_children, char ***cmds)
	{
	  //backup stdin
	  int stdin_bak = dup(0);
	  int child_stdin = 0;
	  int last_stdout = 1; //where output goes
	  int fd[2];
	  char *token;

	  // handle redirection in
	  char redir_detected = 0;
	  for (int i=0; (token=cmds[0][i]) != NULL; i++) {
	    if (!redir_detected && !strcmp(token, "<")) {
	      child_stdin = open(cmds[0][i+1], O_RDONLY);
	      if (child_stdin < 0) {
	        err_sys(cmds[0][i+1]); 
	        return 1;
	      }
	      // remove the "<" token and the file name token that follows from list
	      // of tokens for first cmd
	      cmds[0][i] = NULL;
	      cmds[0][++i] = NULL; // increment i
	      redir_detected = 1;
	    }
	    else if (redir_detected) { // move tokens up 2 in arg
	      cmds[0][i-2] = cmds[0][i];
	      cmds[0][i] = NULL;
	    }
	  }
	  
	  int fg_bak = fg; //save state for last child
	  fg = 0; //children run in background
	  for (int child=0; child<num_children-1; child++) {
	    pipe(fd);
	    //fd[1] is now write end of pipe, fd[0] is read end of pipe
	    program_launch(child_stdin, fd[1], cmds[child]);
	    close(fd[1]); //close the write end fd
	    child_stdin=fd[0]; 
	  }

	  // handle redirection out
	  int last_child = num_children-1;
	  if (child_stdin != 0) dup2(child_stdin, 0);
	  mode_t final_stdout_mode = O_RDWR | O_CREAT;
	  for (int i=0; (token=cmds[last_child][i]) != NULL; i++) {
	    if (!strcmp(token, ">")) {
	      last_stdout = open(cmds[last_child][i+1], final_stdout_mode, 0644);
	      if (child_stdin < 0) {
	        err_sys(cmds[last_child][i+1]); 
	        return 1;
	      }
	      // remove the redirection token and the file name token that follows from list
	      // of tokens for first cmd
	      cmds[last_child][i] = NULL;
	      cmds[last_child][i+1] = NULL; // increment i
	      break;
	    }
	    if (!strcmp(token, ">>")) {
	      last_stdout = open(cmds[last_child][i+1], final_stdout_mode|O_APPEND, 0644);
	      if (child_stdin < 0) {
	        err_sys(cmds[last_child][i+1]); 
	        return 1;
	      }
	      // remove the redirection token and the file name token that follows from list
	      // of tokens for first cmd
	      cmds[last_child][i] = NULL;
	      cmds[last_child][i+1] = NULL; // increment i
	      break;
	    }
	  }

	  fg = fg_bak; //restore fg to backup
	  program_launch(child_stdin, last_stdout, cmds[last_child]);
	  dup2(stdin_bak, 0);
	  close(stdin_bak);
	  return 0;
	} // end of release children

	//if command not found in our cmd list execute its bash command
	int com_execute(char * builtin, char *args){
		int num_builtins = sizeof(functions_str) / sizeof(char*);

		for (int i = 0; i < num_builtins; i++) {
			if (strcmp(builtin, functions_str[i]) == 0){
				return ((*functions_internal[i])(args));
			}
		}
		return 0;
	} // end of com_execute

	//Used http://www.gnu.org for reference and info
	int main(int argc, char const *argv[])
	{
	  // initializing variables
	  char **children, *line, *line_no_spaces;
	  // finding current home directory
	  home = getenv("HOME");
	  int child, num_children;
	  printf("Welcome to Nick Pignone's Shell. Hope you enjoy the show! [Press ENTER to CONTINUE]");
	  // main loop
	  do {
	  	cur_dir_path = getcwd(cur_dir_path, (size_t)1024);
	    // get cwd and write it to beginning of cmd prompt
	    write(STDOUT_FILENO, "\n", 1);
	    if (cur_dir_path) write(STDOUT_FILENO, cur_dir_path, strlen(cur_dir_path));
	    write(STDOUT_FILENO, "> ", 2);

	    // read input from cmd prompt and skip commands that start with white space
	    line = get_line_from_stdin();
	    line_no_spaces = line + strspn(line, " "); 
	    if (!strcmp(line_no_spaces, "\n")) continue;

	    num_children = makeargv(line, "|\n", &children);
     
	    char ***cmds = malloc(num_children*sizeof(char *));
	    for (child=0; child<num_children-1; child++) {
	      makeargv(children[child], " ", &cmds[child]);
	    }

	    // if last child ends with &, then we have to wait
	    int last_token = makeargv(children[child], " ", &cmds[child]) -1;
	    unsigned long last_token_idx = strlen(cmds[child][last_token]) -1;
	    //if ends in &, sets fg (foreground) to 0
	    fg = strcmp(cmds[child][last_token] + last_token_idx, "&");
	    //removes & in the case that there are no spaces between last child and the command
	    if (!fg && last_token_idx) *(cmds[child][last_token]+last_token_idx) = '\0';
	    else if (!fg) cmds[child][last_token] = NULL; //removes & if there are any spaces

	    // if no pipes try to satisfy child with builtin
	    // if child satisfied, then restart loop
	    if (num_children == 1) {
	      int try = com_execute(cmds[0][0], cmds[0][1]);
	      if (try == 1) goto reentry;
	    }
	    release_children(num_children, cmds);

	reentry:
		//freeing children and cmds
	    for(child=0; child<num_children; child++) {
	      free(cmds[child][0]);
	      free(cmds[child]);
	    }
	    free(children[0]);
	    free(children);
	    free(cmds);
	  } while (1); //continue while (1)
		//resets permissions for com_umask function
		reset_perm();
		return EXIT_SUCCESS;
	} //end of main



