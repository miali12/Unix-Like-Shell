#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

struct commandWithArgs{     //struct which holds the command and it's arguments.
    char *fileName;
    char *args[16];
};

int exec(struct commandWithArgs *cmd)     //This function takes in a command with it's arguments in the form of a struct and executes using execvp.
{
    int retval = execvp(cmd->fileName, cmd->args);
    return retval;
}

int pipedCommand(char* cmd)
{
	int ret = 0;
    int numberOfCommands = 0;
    int status;
    pid_t pid;
    char* unParsedCommands[12];     // array which will hold the content between | symbols.
    char* unParsedCopy[12];     // a copy of the unParsedCommands array.
    struct commandWithArgs* commandsWithArgs[12];     //an array which will hold each command with its arguments in the form of a struct. So its an array of struct pointers.
    char* token ;

    token = strtok(cmd,"|");     //parse cmd based on the pipe symbol, and store in unParsedCommands array.
    unParsedCommands[0] = token;
    numberOfCommands++;
    for (int i = 1; token != NULL && i <12; i++)
    {
        token = strtok(NULL, "|");
        unParsedCommands[i] = token;
        numberOfCommands++;
    }
    numberOfCommands--;

    int count = 0;
    while(count < 12 && unParsedCommands[count]!=NULL)     //make a copy of unParsedCommands which we can use without messing up the content of unParsedCommands which will be used later.
    {
        char* copy = malloc(strlen(unParsedCommands[count])+1);
        strcpy(copy,unParsedCommands[count]);
        unParsedCopy[count++] = copy;
    }
    unParsedCopy[count] = NULL;


    for (int i = 0 ; unParsedCopy[i] != NULL && i < 12; i++)     //Now each unParsedCommand will be further parsed into a command and its arguments. 
    {
        char* commandAndArgs[16];
        const char delimiters[] = "<> ";

        token = strtok(unParsedCopy[i], delimiters);
        commandAndArgs[0] = token;

        int j = 1;
        while(token != NULL && j <= 15)
        {
            token = strtok(NULL, delimiters);
            commandAndArgs[j++] = token;

        }
        struct commandWithArgs cwa = {0};     //Now store the command and its arguments in a struct and store the struct in the commandsWithArgs array.
        cwa.fileName = commandAndArgs[0];
        for (int i = 0; i < 16 ; i++)
            cwa.args[i] = commandAndArgs[i];
        commandsWithArgs[i] = &cwa;
    }

    const unsigned int numberOfPipes = numberOfCommands-1;     //create the pipe file descriptors, which are twice as many as there are pipes.
    int pipefd[numberOfPipes*2];
    for (int i = 0; i < numberOfPipes; i++)
        if (pipe(pipefd + i*2) == -1)
            exit(1);

    for (int i = 0 ; unParsedCommands[i] != NULL; i++)     //loop over each command and redirect accordingly, then execute the command.
    {
	pid = fork();
        if (pid == 0)
        {
            if (i != 0)
            {
                if(dup2(pipefd[(i-1)*2], STDIN_FILENO) < 0)      
			perror("error\n");
	    }
	    if (i <numberOfCommands-1)
            {
                if (dup2(pipefd[i*2+1], STDOUT_FILENO) <0)
			perror("error\n");
            }

            for (int i = 0 ; i < numberOfPipes*2; i++)
                close(pipefd[i]);
            
	    exec(commandsWithArgs[i]);

        }
        else if (pid<0)
            exit(1);
    }

    for (int i= 0 ; i < numberOfPipes*2; i++)     //we need to close all pipe file descriptors now.
        close(pipefd[i]);
    for(int i = 0 ; i < numberOfPipes+1; i++)     //wait for all children to finish.
	wait(&status);
	
    return ret;

}


int main(int argc, char *argv[])
{
    char *commandAndArgs[16];     //This array will hold the command and it's possible 15 arguments.
    char cmd[512] ;
    int status;
    int s;     //This is used towards the end of the main function in order to determine whether to print the completed message.

    while(1){
	printf("sshell$ ");
	fgets(cmd, 512, stdin);
	if(!isatty(STDIN_FILENO)){
		printf("%s", cmd);
		fflush(stdout);
	}

	cmd[strlen(cmd)-1] = '\0';	//get rid of \n at the end of cmd.
	
	const char delimiters[] = " &";
	char *token, *cp;     //cp is a copy of cmd.
	
	cp = malloc(strlen(cmd)+1);
	if (cp)
		strcpy(cp, cmd);
	else 
		exit(1);

	token = strtok(cp, delimiters);     //token is being used to parse the cmd.
	commandAndArgs[0] = token;
	
	int i = 1;
	while(token != NULL && i <= 15)
	{
		token = strtok(NULL, delimiters);
		commandAndArgs[i++] = token;
	}
	if ((strcmp(commandAndArgs[0],"exit")) == 0)     //check if the command given was exit, if it was, exit.
	{
		fprintf(stderr, "Bye...\n");
		exit(0);
	}
	else if ((strcmp(commandAndArgs[0],"cd")) == 0)     //check if the command was cd, and do the approriate change of directory.
	{	
	    int success = chdir(commandAndArgs[1]);
	    if (success == -1)
	    {
		    fprintf(stderr, "Error: no such directory\n");
	        s = 1;     //s is set to 1 so that the completion message does not get printed later on.
		}
		else 
		    s = 0;     //s = 0 means that we will go ahead and print the completion message later on.
	}
	else if ((strcmp(commandAndArgs[0],"pwd")) == 0)     //check if the command was pwd...
	{
	    char currDir[512];
		getcwd(currDir, sizeof(currDir));
        fprintf(stdout, "%s\n",currDir);
   		s = 0;			
      	}
	else if (strstr(cmd, "|") !=NULL)     // if the cmd has | in it then we will do piping.
    	{
	    int val;
	    val = pipedCommand(cmd);
	    s = val;
    	}



	else   //since the command was not pwd, cd, or exit, we will go ahead and make a struct with the given command and its arguments and execute using execvp.
	{     
	    struct commandWithArgs * c;
	    struct commandWithArgs cwa = {0};     //cwa is command with arguments.
    	    cwa.fileName = commandAndArgs[0];     // set fileName/command.
    	    for (int i = 0 ; i <16; i++)
		    cwa.args[i] = commandAndArgs[i];     //set the arguments.
    	    c = &cwa;     //c is now a pointer to the struct with the command and its arguments.


    	    pid_t pid;
       	    pid = fork();
    	    if (pid == 0)
     	    {
			if ((strstr(cmd,"<")) != NULL || (strstr(cmd,">"))!= NULL)     //If cmd has redirection in it, we will handle it seperatley.
			{
		   	    char* commandAndArgs[16];
			    const char delimiters[] = "<>";
			    char *token, *cp;

        	   	    cp = malloc(strlen(cmd)+1);    
        		    if (cp)
             	    	      	strcpy(cp, cmd);
        	            else
                    		exit(1);

       		    	    char* arguments;
			    arguments = strtok(cp, delimiters);    
			    token = strtok(arguments, " ");
			    commandAndArgs[0] = token; 

        		    int i = 1;
       			    while(token != NULL && i <= 15)
      			    {
               	   		 token = strtok(NULL, " ");
             		   	 commandAndArgs[i++] = token;
                	    }

			    char* inputFile;
			    inputFile = malloc(strlen(cmd)+1);
			    strcpy(inputFile, cmd); 
			    inputFile = strstr(inputFile,"<");
			    
			    char* ifile = NULL;
			    if (inputFile != NULL)
				 ifile = strtok(inputFile, " <>");	
				
			    char* outputFile;
		            outputFile = strstr(cmd,">");
			    
			    char* ofile = NULL;
			    if (outputFile!=NULL)
				ofile = strtok(outputFile, " >");
			    
			    int fdi;
			    int fdo;
			        
			    if (inputFile != NULL)     //if there was < in the cmd, we try to open up the corresponding file
			    {
				fdi = open(ifile, O_RDONLY);
				if (access(ifile, F_OK| R_OK) != -1)
				{
					dup2(fdi,STDIN_FILENO);
					close(fdi);

				}
				else
				{
					if (ifile == NULL)
						fprintf(stderr, "Error: no input file\n");
					else
						fprintf(stderr, "Error: cannot open input file\n");
					
					exit(2);	
				}
			    }

			    if (outputFile != NULL)     //if there was > in cmd we try to open up corresponding file.
		            {
				fdo = open(ofile, O_WRONLY | O_CREAT, S_IRUSR | O_TRUNC | S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);     //if the file does not exsist, create it.
				if (access(ofile, F_OK | W_OK) != -1)
				{
					dup2(fdo, STDOUT_FILENO);
					close(fdo);
				}
				else
				{
					if (ofile == NULL)
						fprintf(stderr, "Error: no output file\n");
					else
						fprintf(stderr, "Error: cannot open output file\n");
					exit(2);
				}
		 	    }

			    struct commandWithArgs* c ;      
		    	    struct commandWithArgs cwa = {0};
			    cwa.fileName = commandAndArgs[0];
			    for (int i = 0; i < 16 ; i++)
		  		cwa.args[i] = commandAndArgs[i];
		    	    c = &cwa;
		  	    exec(c) ;
			    exit(2);
			
		    	}
	        	else  if (((strcmp(commandAndArgs[0],"pwd")) != 0) && (strcmp(commandAndArgs[0],"cd") != 0) && (( strstr(cmd,"<")) == NULL) && ((strstr(cmd, ">")) == NULL))
        		{
               		     exec(c);
                    	     fprintf(stderr, "Error: command not found\n");
                     	     s = 1;
               		}
            }
	    else if (pid<0)
	        exit(1);
	    else 
 	    {
	        if (strstr(cmd, "&") == NULL)
	        {
	   	    waitpid (pid, &status, 0);
		    if (WIFEXITED(status))
		        s = WEXITSTATUS(status);
	        }   
	    } 
	}
 	if (s != 2)
	    fprintf(stderr, "+ completed '%s' %c%d%c\n", cmd, '[', s, ']');
	
	
    }
	
    return EXIT_SUCCESS;

}
