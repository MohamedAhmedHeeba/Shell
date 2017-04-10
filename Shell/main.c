#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#define MAX_LINE 80 /* The maximum length command */
#define MAX_PATH 20
#define BUFFERSIZE 1000

int size = 0;
int paths_size = 0;
char *history[10];

char *ltrim(char *s){
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s){
    char* back;
    int len = strlen(s);

    if(len == 0)
        return(s);

    back = s + len;
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

char *trim(char *s){
    return rtrim(ltrim(s));
}

char *strsub(char* str, int start, int length){
    char *sub = malloc(length);
    int c = 0;
    while (c < length) {
        sub[c] = str[start+c];
        c++;
    }
    sub[c] = '\0';
    return sub;
}

char *concatenate(char* str1, char* str2){
    char *str3 = (char *) malloc(strlen(str1)+ strlen(str2) +1);
    strcpy(str3, str1);
    strcat(str3, str2);
    return str3;
}

char *read_command(){
    char *text = calloc(1,1), buffer[BUFFERSIZE];
    if(fgets(buffer, BUFFERSIZE , stdin)==NULL){finish();}
    text = realloc( text, strlen(text)+1+strlen(buffer) );
    if( !text ) {printf("Error!!!");}/* error handling */
    strcat( text, buffer ); /* note a '\n' is appended here everytime */
    return text;
}

void echo_handle(char *line){
    char *arg = strsub(line,4,strlen(line)-4);
    arg = trim(arg);
    int len = strlen(arg);
    int i, in_sq = 0, in_dq = 0, b_slash = 0, space = 0;
    for(i = 0 ; i < len ; i++){
        if(arg[i]=='\'' && in_dq==0){
            if(in_sq == 0){in_sq = 1;}
            else{in_sq = 0;}

            continue;
        }
        if(arg[i]=='"'&& in_sq==0){
            if(in_dq == 0){in_dq = 1;}
            else{in_dq = 0;}
            continue;
        }
        if(in_sq == 1 || in_dq == 1){
            if(arg[i]=='\\'){
                b_slash++;

            }else{
                int j;
                if(b_slash>0)for(j = 0 ; j < b_slash/2+1 ;j++){printf("\\");}
                printf("%c",arg[i]);
                b_slash = 0;
            }
        }else{
            if(arg[i]==' '||arg[i]=='\t'){
                if(space==0){
                    printf(" ");
                    space = 1;
                }
            }else if(arg[i]=='\\'){
                b_slash++;
            }else{
                int j;
                for(j = 0 ; j < b_slash/2 ;j++){printf("\\");}
                printf("%c",arg[i]);
                space = 0;
                b_slash = 0;
            }
        }
    }
    printf("\n");
}

int get_args(char *line, char *args[]){
    char* echo;
    line =concatenate(trim(line),"");
    if(strcmp(line,"")==0){return 0;}
    if(line[strlen(line)-1]=='\n'){line[strlen(line)-1] ='\0';}
    add_to_history(concatenate(line,""));
    if(strcmp(line,"history")==0){
        print_history();
        return 0;
    }
    echo = strsub(line,0,4);
    if(strcmp(echo,"echo")==0){
        echo_handle(line);
        return 0;
    }
    char * token;
    size = 0;
    token = strtok (line," \t");
    if(strcmp(token,"\n")==0){return 0;}
        while (token != NULL){
            if(token[strlen(token)-1]=='\n'){
                token[strlen(token)-1] ='\0';
            }
            if(strcmp(token,"\t")!=0 && strcmp(token," ")!=0
                && strcmp(token,"\n")!=0 && strcmp(token,"")!=0
                &&token!=NULL){
                args[size++] = token;
            }
            token = strtok (NULL, " \t");
        }
        args[size] = NULL;

        if(strcmp(args[0],"exit")==0 ){
           finish();
        }
        return 1;
}

int parse_histroy(char *line, char *args[]){
    line = concatenate(trim(line),"");
    if(strcmp(line,"")==0){return 0;}
    if(line[strlen(line)-1]=='\n'){
        line[strlen(line)-1] ='\0';
    }
    if(line[0]=='!'){
        if(line[1]=='!'){
            if(history[0]==NULL){
                printf("No commands in history!!!\n");
                return 0;
            }
            return get_args(history[0],args);
        }else if(line[1]>='0'&&line[1]<='9'){
            if(history[line[1]-'0']==NULL){
                printf("No such command in history!!!\n");
                return 0;
            }
            return get_args(history[line[1]-'0'],args);
        }
    }
    if(strcmp(line,"history")==0){
        add_to_history(concatenate(line,""));
        print_history();
        return 0;
    }
    if(strcmp(line,"exit")==0 ){
        add_to_history(concatenate(line,""));
        finish();
    }
    return -1;
}

int check_ampersand(char *args[]){
    char *last_word = args[size-1];
    int len = strlen(last_word);
    char last_char = last_word[len-1];
    // delete & form agrs
    if(last_char=='&'){
        if(len==1){
            args[size-1] = NULL;
            size--;
        }else if(len > 1){
            last_word[len-1] = '\0';
        }
        return 1;
    }
    return 0;
}

void find_paths(char *paths[]){
    char *path_string = getenv("PATH");
    char *token ;
    token = strtok(path_string ,":");
    while(token != NULL){
        paths[paths_size++] = token;
        token = strtok(NULL ,":");
    }
}

char *check_vaild_path(char *args_zero, char *paths[]){
    int i;
    char *path;
    for(i = 0 ; i < paths_size ; i++){
        path = NULL;
        path = concatenate(paths[i], "/");
        path = concatenate(path, args_zero);
        int flag = access(path , F_OK);
        if(flag != -1){
            return path;
        }
    }
    return args_zero;
}

int forkProcess(char *args[], char *paths[]){
    pid_t pid;
    int status;
    int wait_flag = check_ampersand(args);
    pid = fork();
    if(pid < 0){
        printf(stderr,"Fork Failed!!");
    }else if (pid == 0){ /* child */
        return execv(check_vaild_path(args[0],paths),args);
    } else if(wait_flag==1){ /* parent */
        waitpid(pid, &status, 0);
        return 1;
    }else if(wait_flag==0){
        return 1;
    }
}

void read_execute_file(char *file_name, char *args[], char *paths[]){
    FILE* file = fopen(file_name, "r"); /* should check the result */
    if (file == NULL){
        printf("Error opening file!\n");
        exit(1);
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != NULL) {
        if(line == EOF || line == NULL){
            fclose(file);
            finish();
        }
        printf("line=> %s\n", line);
        int flag = parse_histroy(line,args);
        if(flag==1){
            if(forkProcess(args,paths)==-1){
                printf("%s : command not found!!\n",line);
                continue;
            }
        }else if(flag==0){
        }else if(get_args(line,args)==1){
            if(forkProcess(args,paths)==-1){
                printf("%s : command not found!!\n",line);
                continue;
            }
        }
    }
    fclose(file);
    finish();
}

void add_to_history(char *command){
    int i;
    for(i = 8 ; i >= 0 ; i--){
        history[i+1] = history[i];
    }
    history[0] = concatenate(command,"");
}

void save_history(){
    FILE *file = fopen("history.txt", "w");
    if (file == NULL){
        printf("Error opening file!\n");
        exit(1);
    }
    int i;
    for(i = 0 ; i < 10 ; i++){
        char *line = history[i];
        if(line != NULL && strcmp(line,"(null)")){
            fprintf(file, "%s\n", line);
        }
    }
    fclose(file);
}

void load_history(){
    FILE* file = fopen("history.txt", "r");
    if (file == NULL){
        printf("Error opening file!\n");
        exit(1);
    }
    char line[MAX_LINE];
    int i = 0;
    while (fgets(line, sizeof(line), file) != NULL){
        if(strcmp(line,"")!=0 && strcmp(line,"\n")!=0 && line != NULL){
            history[i++] = concatenate(line,"");
        }
    }
    fclose(file);
}

void finish(){
    save_history();
    exit(0);
}

void print_history(){
    int i;
    for(i = 0 ; i < 10 ; i++){
        char *line =  history[i];
        if(line != NULL){
            printf("%d %s\n", i, line);
        }
    }
}

void handle_method(int signal){
    pid_t pid;
    while((pid = waitpid(-1,NULL,WNOHANG)) > 0){
        printf("handler reaped child %d\n",(int)pid);
    }
    //if(errno != ECHILD){printf("waitpid error!!\n");}
}

int main(int argc, char *argv[]){
    if(signal(SIGCHLD, handle_method)==SIG_ERR){printf("signal error!!");}
    load_history();
    char *args[MAX_LINE/2 + 1]; /* command line arguments */
    int should_run = 1; /* flag to determine when to exit program */
    char *paths[MAX_PATH];
    find_paths(&paths);
    while (should_run){
        if(argc > 1){ //read from file
            read_execute_file(argv[1],&args,&paths);
        }else{
            printf("shell>");
            char *line = read_command();
            if(strlen(line)>80){
                printf("A very long command line (over 80 characters)\n");
                continue;
            }
            int flag = parse_histroy(line,&args);
            if(flag==1){
                if(forkProcess(&args,&paths)==-1){
                    printf("%s : command not found!!\n",line);
                    continue;
                }
            }else if(flag==0){
            }else if(get_args(line,&args)==1){
                if(forkProcess(&args,&paths)==-1){
                    printf("%s : command not found!!\n",line);
                    continue;
                }
            }
        }
    }
    return 0;
}
