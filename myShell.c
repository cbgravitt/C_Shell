#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <fcntl.h> 
#include <errno.h>
void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}
void myError(){
    char error_msg[30] = "An error has occurred\n";
    myPrint(error_msg);
}
void myPwd(){
    char directory[200];
    myPrint(getcwd(directory, 200));
    myPrint("\n");
}
char* insertSpace(char* str){
    //inserts a space before and after > or >+
    //if there is already at least one space before/after it returns as-is
    //also returns as-is if no > or >+ is found
    char* temp = (char*)malloc(sizeof(char*) * (strlen(str) + 3));
    int index = strcspn(str, ">");
    if((index == strlen(str)) || ((str[index - 1] == ' ') && ((str[index + 1] == ' ') ||
      ((str[index + 1] == '+') && (str[index + 2] == ' '))))){
        return str;
    }
    for(int i = 0; i < strlen(str); i++){
        if(i < index){
            temp[i] = str[i];
        }else if(i > index){
            temp[i + 2] = str[i];
        }else if (i == index){
            temp[i] = ' ';
            temp[i + 1] = '>';
            temp[i + 2] = ' ';
            if(str[i + 1] == '+'){
                temp[i + 2] = '+';
                temp[i + 3] = ' ';
                i++;
            }
        }
    }
    temp[strlen(str) + 3] = '\0';   
    return temp;
}
int countInputs(char* input){
    int count = 0;
    char* save = NULL;
    char* temp;
    temp = strtok_r(input, "\n;", &save);
    for(int i = 0; i < strlen(input); i++){
        while(temp != NULL){
            count++;
            temp = strtok_r(NULL, "\n;", &save);
        }
    }
    return count;
}
int countArgs(char* input){
    int count = 0;
    char* temp;
    char* save;
    temp = strtok_r(input, " \t", &save);
    for(int i = 0; i < strlen(input); i++){
        while(temp != NULL){
            count++;
            temp = strtok_r(NULL, " \t", &save);
        }
    }
    return count;
}
void myCd(char* dir){
    if(dir == NULL){
        chdir(getenv("HOME"));
    }else{
        if(chdir(dir) == -1){
            myError();
            return;
        }
    }
}
int contains_redir(char** args, int arg_count){
    for(int i = 0; i < arg_count; i++){
        if((strcmp(args[i], ">") == 0) || (strcmp(args[i], ">+") == 0)){
            return 1;
        }
    }
    return 0;
}
void redirect(char** args, int arg_count){
    int index = 0;
    int status;
    for(int i = 0; i < arg_count; i++){
        if((strcmp(args[i], ">") == 0) || (strcmp(args[i], ">+") == 0)){
            index = i;
            if((args[i + 1] == NULL) || (args[i + 2] != NULL)){
                myError();
                //myPrint("Invalid arguments\n");
                return;
            }
        }
    }
    if(strcmp(args[index], ">") == 0){
        if(access(args[index + 1], F_OK) == 0){
            //the file exists already
            myError();
            //myPrint("file already exists\n");
            return;
        }else{
            int ret = fork();
            if(ret == 0){
                int output_fd = open(args[index + 1], O_RDWR | O_CREAT, 00700);
                if(output_fd == -1 && errno == ENOENT){
                    myError();
                    exit(0);
                }
                dup2(output_fd, STDOUT_FILENO);
                args[index] = NULL;
                if(execvp(args[0], args) == -1){
                    myError();
                    //myPrint("execvp failed\n");
                    exit(0);
                }
            }else{
                waitpid(ret, &status, 0);
                return;
            }
        }
    }else{ //advanced redirect
        if(access(args[index + 1], F_OK) == 0){
            //the file exists already
            int r = 0;
            char* buffer = (char*)malloc(sizeof(char) * 200);
            //buffer[200] = '\0';
            int temp_fd = open("temp", O_RDWR | O_CREAT, 00700);
            int original_fd = open(args[index + 1], O_RDWR);
            while((r = read(original_fd, buffer, 200)) != 0){
                write(temp_fd, buffer, r);
            }
            write(temp_fd, buffer, r); //temp_fd is now a copy of the original
            /* myPrint("first buffer: ");
            myPrint(buffer); */
            close(original_fd);
            int ret = fork();
            if(ret == 0){//child
                int output_fd = open(args[index + 1], O_RDWR | O_TRUNC);
                if(output_fd == -1 && errno == ENOENT){
                    myError();
                    exit(0);
                }
                dup2(output_fd, STDOUT_FILENO);
                args[index] = NULL;
                if(execvp(args[0], args) == -1){
                    myError();
                    //myPrint("execvp failed\n");
                    exit(0);
                }
            }else{
                waitpid(ret, &status, 0);
                int final_fd = open(args[index + 1], O_WRONLY | O_APPEND);
                lseek(temp_fd, 0, SEEK_SET);
                while((r = read(temp_fd, buffer, 200)) != 0){
                    write(final_fd, buffer, r);
                }
                write(final_fd, buffer, r);
                free(buffer);
                close(final_fd);
                close(temp_fd);
                remove("temp");
                return;
            }
        }else{ //if the file doesn't exist behaves the same as normal redirect
            int ret = fork();
            if(ret == 0){
                int output_fd = open(args[index + 1], O_RDWR | O_CREAT, 00700);
                if(output_fd == -1 && errno == ENOENT){
                    myError();
                    exit(0);
                }
                dup2(output_fd, STDOUT_FILENO);
                args[index] = NULL;
                if(execvp(args[0], args) == -1){
                    myError();
                    //myPrint("execvp failed\n");
                    exit(0);
                }
            }else{
                waitpid(ret, &status, 0);
                return;
            }
        }
    }
}
void runProgram(char** args){
    int ret = fork();
    int status;
    if(ret == 0){
        if(execvp(args[0], args) == -1){
            myError();
            //myPrint("runProgram: execve failed\n");
            exit(0);
        }
    }else{
        waitpid(ret, &status, 0);
        return;
    } 
}
int hasChar(char* str){
    int ret = 0;
    for(int i = 0; i < strlen(str); i++){
        if((str[i] != ' ') && (str[i] != '\t') && (str[i] != '\n')){
            ret = 1;
        }
    }
    return ret;
}
int main(int argc, char *argv[]) 
{
    char *pinput;
    char cmd_buff[514];
    char buff_cpy1[514];
    char buff_cpy2[514];
    int open = 0;
    while (1) {
        if(argv[1] == NULL){
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, 514, stdin);
            if(!strchr(cmd_buff, '\n')){
                myError();
                while(fgetc(stdin) != '\n');
                continue;
            }
            if (!pinput) {
                exit(0);
            }
        }else if(argv[2] != NULL){
            myError();
            //myPrint("argv[2] is NULL\n");
            exit(0);
        }else{
            char* file_buf = NULL;
            size_t length;
            long int read;
            FILE* file;
            if(open == 0){
                file = fopen(argv[1], "r");
                open = 1;
            }
            if(file == NULL){
                myError();
                //myPrint("file is null\n");
                exit(0);
            }
            read = getline(&(file_buf), &(length), file);
            if(read == -1){
                exit(0); //getline fails
            }
            strcpy(cmd_buff, file_buf);
            if(hasChar(cmd_buff)){
                myPrint(cmd_buff);
            }
            if(length > 512){
                myError();
                continue;
            }
        }
        
        char* input = NULL;
        strcpy(buff_cpy1, cmd_buff);
        strcpy(buff_cpy2, cmd_buff);
        char* inputs[countInputs(buff_cpy2)];
        char* save_pointer1 = NULL;
        input = strtok_r(buff_cpy1, "\n;", &save_pointer1);
        int count = 0;
        while(input != NULL){
            inputs[count] = insertSpace(input);
            count++;
            input = strtok_r(NULL, "\n;", &save_pointer1);
        }
       /*  myPrint("inputs:\n");
            for(int i = 0; i < count; i++){
                myPrint(inputs[i]);
                myPrint("\n");
            } */
        free(input);
        
        for(int i = 0; i < count; i++){
            char* input_cpy = strdup(inputs[i]);
            char* save_pointer2 = NULL;
            char** args = (char**)malloc(sizeof(char*) * (countArgs(input_cpy) + 1));
            free(input_cpy);
            char* arg = strtok_r(inputs[i], " \t", &save_pointer2);
            /* myPrint(arg);
            myPrint("\n"); */
            int arg_count = 0;
            while(arg != NULL){
                args[arg_count] = (char*)malloc(sizeof(char*));
                args[arg_count] = arg;
                arg_count++;
                arg = strtok_r(NULL, " \t", &save_pointer2);
            }
            args[arg_count] = NULL;
            /* myPrint("args:\n");
            for(int i = 0; i < arg_count; i++){
                myPrint(args[i]);
                myPrint("\n");
            } */
            for(int j = 0; j < arg_count; j++){
                if(strcmp(args[j], "exit") == 0){
                    if(args[j + 1] != NULL){
                        myError();
                        break;
                    }
                    exit(0);
                }else if(strcmp(args[j], "pwd") == 0){
                    if(args[j + 1] != NULL){
                        myError();
                        break;
                    }else{
                        myPwd();
                    }
                }else if(strcmp(args[j], "cd") == 0){
                    if((args[j + 1] != NULL) && (args[j + 2] != NULL)){
                        myError();
                        break;
                    }else if(contains_redir(args, arg_count)){
                        myError();
                        break;
                    } else{
                        myCd(args[++j]); 
                    }   
                }else{
                    if(contains_redir(args, arg_count)){
                        redirect(args, arg_count);
                        break;
                    }else{
                        runProgram(args);
                        break;
                    }
                }
            }
        }
    }       
}