#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

int nj=0;                                       //The number of jobs
char ** com;                                    //The command of jobs
pid_t ** lpid;                                  // The pid of jobs

int countpipe=0;                                //The number of process in pipes
pid_t *  pipepid;                               //The pid of process above
int max=0;                                      //The signal of first job



//The cd command
int cd(char **args){
    if (args[1] == NULL || args[2] != NULL){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    else{
        if (chdir(args[1]) != 0){
            fprintf(stderr, "Error: invalid directory\n");
            return 1;
        }
    }
    return 1;
}




//The exit command
int nyush_exit(char **args){
    //If exists jobs
    if (nj!=0 && args!=NULL){
        fprintf(stderr, "Error: there are suspended jobs\n");
        return 1;
    }
    else{
        free(com);
        free(lpid);
        return 0;
    }
}





//The jobs command--print jobs
int nyush_jobs(char **args){
    if (args[1] != NULL){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    else{
        char ** jobs=malloc(nj*sizeof(char *));
        int i;
        for (i = 0;i<nj;i++){
            jobs[i]=malloc(1000*sizeof(char));
            sprintf(jobs[i], "[%d] %s", i+1 , com[i]);
        }
        for (i = 0;i<nj;i++){
            printf("%s",jobs[i]);
        }
        for (i = 0;i<nj;i++){
            free(jobs[i]);
        }
        free(jobs);
        
    }
    return 1;
}



//The fg command
int fg(char **args){
    if (args[1] == NULL || args[2] != NULL){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    else{
        int fg_num;
        fg_num = atoi(args[1]);
        
        //fg n>nj
        if (fg_num>nj){
            fprintf(stderr, "Error: invalid job\n");
            return 1;
        }
        else{
            //Get needed command
            char * fgcom=malloc(sizeof(char)*1000);
            int i=0;
            while(com[fg_num-1][i]){
                fgcom[i]=com[fg_num-1][i];
                i++;
            }
            fgcom[i]='\0';
        
            int c;
            //Update command
            for(i=fg_num;i<nj;i++){
                c =0;
                while(com[i][c]!='\0'){
                    com[i-1][c]=com[i][c];
                    c++;
                }
                com[i-1][c]='\0';
            }
            free(com[i-1]);
            
            
            
            //Get needed pid
            pid_t * fgpid=malloc(sizeof(pid_t)*1000);
            i=0;
            while(lpid[fg_num-1][i]!=0){
                fgpid[i]=lpid[fg_num-1][i];
                i++;
            }
            fgpid[i]=0;
            
            
            //Update pidlist
            for(i=fg_num;i<nj;i++){
                c =0;
                while(lpid[i][c]){
                    lpid[i-1][c]=lpid[i][c];
                    c++;
                }
                lpid[i-1][c]=0;
            }
            free(lpid[i-1]);
            
            
            
            //Continue the processes
            nj--;
            i=0;
            while(fgpid[i]!=0){
                kill(fgpid[i], SIGCONT);
                i++;
            }
            
            //Realloc command and pidlist
            com=realloc(com,(nj+1)*sizeof(char *));
            com[nj]=NULL;
            lpid=realloc(lpid,(nj+1)*sizeof(pid_t *));
            lpid[nj]=NULL;
            
            
            //Deal with new stop process
            int status = 0;
            waitpid(fgpid[i-1], &status, WUNTRACED);
            
            
            //If it term/exit, kill other pipe processes(zombies)
            if (!WIFSTOPPED(status)){
                int j;
                for (j=0;j<i-1;j++){
                    waitpid(fgpid[j], NULL, 0);
                }
            }
            
            
            //If it just stop again, add back to jobs
            else {
                nj++;
                //Realloc command and pidlist
                com=realloc(com,(nj+1)*sizeof(char *));
                com[nj]=NULL;
                lpid=realloc(lpid,(nj+1)*sizeof(pid_t *));
                lpid[nj]=NULL;
                
                //Add a command
                com[nj-1]=malloc(1000*sizeof(char));
                i=0;
                while(fgcom[i]){
                    com[nj-1][i]=fgcom[i];
                    i++;
                }
                com[nj-1][i]='\0';
                
                //Add a pid
                lpid[nj-1]=malloc(1000*sizeof(pid_t));
                i=0;
                while(fgpid[i]!=0){
                    lpid[nj-1][i]=fgpid[i];
                    i++;
                }
                lpid[nj-1][i]=0;
            }
            
            //Free temporary command and pid
            free(fgcom);
            free(fgpid);
        }
    }
    return 1;
}




int launch(int pipe_in, int pipe_out, int start, int end, char** args, char* line){
    pid_t pid;
    int status = 0;
    pid = fork();
    if (pid == 0){
    //The Child process
        
        //The I/O redirection
        int len = 0;
        while(args[len]){
            len++;
        }
        char *args_clean[len];
        int idx = 0;
        int cleanidx = 0;
        int in, out, append;

        while (args[idx]) {
            //The "<"
            if (!strcmp(args[idx], "<")) {
                idx++;
                if ((in = open(args[idx], O_RDONLY)) < 0) {
                    fprintf(stderr, "Error: invalid file\n");
                    exit(-1);
                }
                dup2(in, STDIN_FILENO);
                close(in);
                idx++;
                continue;
            }
            
            //The ">"
            if (!strcmp(args[idx], ">")) {
                idx++;
                out = creat(args[idx], 0600);
                dup2(out, STDOUT_FILENO);
                close(out);
                idx++;
                continue;
            }

            //The ">>"
            if (!strcmp(args[idx], ">>")) {
                idx++;
                if ((append = open(args[idx], O_CREAT | O_RDWR | O_APPEND, 0600)) < 0) {
                    fprintf(stderr, "Error: invalid file\n");
                    exit(-1);
                }
                dup2(append, STDOUT_FILENO);
                close(append);
                idx++;
                continue;
            }
            args_clean[cleanidx++] = args[idx];
            ++idx;
        }
        //Update the args(remove "<" ">" and ">>")
        args_clean[cleanidx] = NULL;
        args=args_clean;
        
        
        
        //The pipes handler(if no I/O redirection)
        if (end!=-2 && start!=-100 && pipe_in!=0){   //Not the first or last pipe
            dup2 (pipe_in, STDIN_FILENO);
            close (pipe_in);
            dup2 (pipe_out, STDOUT_FILENO);
            close (pipe_out);
        }
        else if (end!=-2 && start==-100){            //The last pipe
            dup2 (pipe_in, STDIN_FILENO);
            close (pipe_in);
        }
        else if (end!=-2 && pipe_in==0){             //The first pipe
            dup2 (pipe_out, STDOUT_FILENO);
            close (pipe_out);
        }
        
        
        
        //Locating programs
        
        //An absolute path
        if (args[0][0] == '/'){
            if (execv(args[0], args) == -1){
              fprintf(stderr, "Error: invalid program\n");
            }
        }
        
        //A relative path without "./"
        else if (args[0][0] != '.' && args[0][0] != '/' && strchr(args[0], '/') !=NULL){
            char bin[40];
            strcpy(bin, "./");
            strcat(bin,args[0]);
            args[0] = bin;
            if (execv(args[0], args) == -1){
                fprintf(stderr, "Error: invalid program\n");
            }
        }
        
        //A relative path with "./"
        else if (args[0][0] == '.'){
            if (execv(args[0], args) == -1){
                fprintf(stderr, "Error: invalid program\n");
            }
        }
        
        //The base name only program
        else{
            char bin[40];
            strcpy(bin, "/bin/");
            strcat(bin,args[0]);
            if (execv(bin, args) == -1){
                char bin1[40];
                strcpy(bin1, "/usr");
                strcat(bin1,bin);
                if (execv(bin1, args) == -1){
                    fprintf(stderr, "Error: invalid program\n");
                }
            }
        }
        exit(-1);
    }
    
    
    else{
    //The Parent process
        
        //Count and Store pipepid
        if(end!=-2){
            if(countpipe==0){
                countpipe++;
                pipepid=malloc((countpipe+1)*sizeof(pid_t));
                pipepid[countpipe-1]=pid;
                pipepid[countpipe]=0;
            }
            else{
                countpipe++;
                pipepid=realloc(pipepid,(countpipe+1)*sizeof(pid_t));
                pipepid[countpipe-1]=pid;
                pipepid[countpipe]=0;
            }
        }
        
        
        //The wait
        
        //If last pipe process
        if(end!=-2 && start==-100){
            waitpid(pid, &status, WUNTRACED);
            
            //If it term/exit, kill other pipe processes(zombies)
            if (!WIFSTOPPED(status)){
                int i=0;
                for (i=0;i<countpipe;i++){
                    waitpid(pipepid[i], NULL, 0);
                }
            }
        }
        
        
        //If normal processes
        else if(end ==-2){
            waitpid(pid, &status, WUNTRACED);
        }
        
        
        //The stop process
        if (WIFSTOPPED(status)){
//            No need to stop other pipe processes(if one stop, all block)
//            printf("%d\n",nj);
//            if (end!=-2){
//                int j=0;
//                while(pipepid[j]!=0){
//                    kill(pipepid[j],SIGTSTP);
//                    j++;
//                }
//            }
            
            //Update jobs
            
            //Update command/pid list
            nj++;
            if (max==0){
                com=malloc((nj+1)*sizeof(char *));
                lpid=malloc((nj+1)*sizeof(pid_t *));
            }
            else{
                com=realloc(com,(nj+1)*sizeof(char *));
                lpid=realloc(lpid,(nj+1)*sizeof(pid_t *));
            }
            max++;
            com[nj]=NULL;
            lpid[nj]=NULL;
            
            
            //New command
            com[nj-1]=malloc(1000*sizeof(char));
            int i=0;
            while(line[i]){
                com[nj-1][i]=line[i];
                i++;
            }
            com[nj-1][i]='\0';
            
            
            //New pid
            
            lpid[nj-1]=malloc(1000*sizeof(pid_t));
            //Single pid
            if(end==-2){
                lpid[nj-1][0]=pid;
                lpid[nj-1][1]=0;
            }
            //Pipe pids
            else{
                int i=0;
                while(pipepid[i]){
                    lpid[nj-1][i]=pipepid[i];
                    i++;
                }
                lpid[nj-1][i]=0;
            }
        }
    }
    return 1;
}




int fork_pipes (char** args, char* line){
    int i=0;
    int in;                                             //The input pipe
    int start;                                          //The start of cammand
    int end  = -2;                                      //The end of cammand
    char * args_end[1000];
    int count=0;
    
    in = 0;                                             //The first pipe
    
    while (args[i]){
        if (!strcmp(args[i], "|")){
            count++;
        }
        i++;
    }
    
    int ** fd=malloc(sizeof(int *)*count);              //The pipes
    
    i=0;
    count=0;
    
    while (args[i]){
        
        if (!strcmp(args[i], "|")){
            count++;
            // "|" is the last
            if (args[i+1]==NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            start=end+2;
            end=i-1;
            // "|" is the first
            if (end==-1){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            
            
            fd[count-1]=malloc(2*sizeof(int));
            pipe(fd[count-1]);
            
            //Small command piece
            memset( args_end, 0, sizeof(char *)*1000 );
            int j;
            for (j = start; j<=end ; j++){
                args_end[j-start]=args[j];
            }
            args_end[j+1]=NULL;
            
            //Run small command piece
            j = launch (in, fd[count-1][1], start, end, args_end,line);
            close(fd[count-1][1]);
            
            //Next pipe's input
            in = fd[count-1][0];
        }
        i++;
    }
    
    
    //The last pipe/command
    memset( args_end, 0, sizeof(char *)*1000 );
    for (i = end+2; args[i] ; i++){
        args_end[i-(end+2)]=args[i];
    }
    args_end[i+1]=NULL;
    start = -100;                                   //The last signal
    int xx=0;
    
    
    free(fd);                                       //Important
    launch(in, xx, start,end,args_end,line);
    
    //Free pipepid
    free(pipepid);
    countpipe=0;
    return 1;
}




//The execute
int execute(char** args, char* line){

    if (args[0] == NULL) {
    // An empty command was entered.
    return 1;
    }
    
    //The Error: cat < output.txt < output2.txt
    int j=0;
    int conti = 0;
    int conto = 0;
    int contoo = 0;
    int conterror = 0;
    while (args[j]){
        if (!strcmp(args[j], "<")){
            if (args[j+1]==NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            conti++;
            if (args[j+2]!=NULL){
                if (strcmp(args[j+2], "|") && strcmp(args[j+2], ">") && strcmp(args[j+2], ">>")){
                    fprintf(stderr, "Error: invalid command\n");
                    return 1;
                }
            }
        }
        else if (!strcmp(args[j], ">")){
            if (args[j+1]==NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            conto++;
            if (args[j+2]!=NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
        }
        else if (!strcmp(args[j], ">>")){
            if (args[j+1]==NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
            contoo++;
            if (args[j+2]!=NULL){
                fprintf(stderr, "Error: invalid command\n");
                return 1;
            }
        }
        else if (!strcmp(args[j], "<<")){
            conterror++;
        }
        j++;
    }
    
    if (conti>1 || conto>1 || contoo >1 || conterror>0){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    
    
    //The Error: cat | cat < input.txt
    j=0;
    conti=0;
    int contp=0;
    while (args[j]){
        if (!strcmp(args[j], "|")){
            contp++;
        }
        else if (!strcmp(args[j], "<") && (contp>0)){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        j++;
    }
    
    
    //The Error: cat > output.txt | cat
    j=0;
    conto=0;
    contoo=0;
    while (args[j]){
        if (!strcmp(args[j], ">")){
            conto++;
        }
        else if (!strcmp(args[j], ">>")){
            contoo++;
        }
        else if (!strcmp(args[j], "|") && ((conto>0) || (contoo>0)) ){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        j++;
    }
    
    
    
    //The Execute
    
    //The Fork
    if (contp>0) {
        return fork_pipes(args,line);
    }
    
    //The Build-in command
    size_t i;
    char *builtin_str[] = {"cd","exit","jobs","fg"};
    int (*builtin_func[]) (char **) = {&cd,&nyush_exit,&nyush_jobs,&fg};

    for (i = 0; i < (sizeof(builtin_str) / sizeof(char *)); i++){
        if (strcmp(args[0], builtin_str[i]) == 0){
          return (*builtin_func[i])(args);
        }
    }
    
    //The Program
    int out = 0;
    int start = 0;
    int in = 0;
    int end = -2;
    return launch(in,out,start,end,args,line);
}


//Split the line
#define LSH_TOK_BUFSIZE 1000
#define LSH_TOK_DELIM " \t\r\n\a"
char **split_line(char *line)
{
    int bufsize = LSH_TOK_BUFSIZE, position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL)
    {
        tokens[position] = token;
        position++;
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}



//Read line
char *read_line(void)
{
    size_t bufsize = 1000;
    char *line = (char *)malloc(bufsize * sizeof(char));
    if(getline(&line, &bufsize, stdin)==-1){
        printf("\n");
        exit(-1);
    }
    return line;
}


//The prompt
void dir(){
    char cwd[1024];
    char * cwd_base = NULL;
    getcwd(cwd, sizeof(cwd));
    cwd_base = basename(cwd);
    fflush(stdout);
    printf("[nyush %s]$ ", cwd_base);
    fflush(stdout);
}



//The loop
void loop(void)
{
    char *line;
    char **args;
    int status;

    do {
        //The prompt
        dir();
        
        char *lines=malloc(1000 * sizeof(char));
        line = read_line();
        strcpy(lines,line);
        args = split_line(lines);
        status = execute(args, line);

        free(line);
        free(args);
        free(lines);
    } while (status);
}



//The signal handler
void handler(){
    printf("\n");
    fflush(stdout);
}


//The main function
int main(void)
{
    signal(SIGINT, handler);
    signal(SIGQUIT, handler);
    signal(SIGTERM, handler);
    signal(SIGTSTP, handler);
    loop();
    exit(-1);
}

