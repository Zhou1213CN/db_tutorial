#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define InputBuffer as a small wrapper around the 
// state we need to store to interact with getline()
typedef struct {
    // char* 表示字符指针类型，当其指向一个字符串的第一个元素时，就可以表示这个字符串。 
    char* buffer;
    // typedef unsigned int size_t;为无符号整型
    size_t buffer_length;
    // ssize_t是C语言头文件stddef.h中定义的类型。
    // ssize_t是有符号整型，在32位机器上等同与int，在64位机器上等同与long int.
    ssize_t input_length;
} InputBuffer;

// 枚举是 C 语言中的一种基本数据类型，用于定义一组具有离散值的常量
// 第一个枚举成员的默认值为整型的 0，后续枚举成员的值在前一个成员上加 1。
typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

// 省略枚举名称，直接定义枚举变量
typedef enum { PREPARE_SUCCESS, PREPARE_UNRECOGNIZED_STATEMENT } PrepareResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
    StatementType type;
} Statement;

// Initialize a InputBuffer pointer
InputBuffer* new_input_buffer(){
    // 指针也就是内存地址，指针变量是用来存放内存地址的变量。
    // malloc() 函数从堆内存中申请size个字节的内存，返回一个指针，指向已分配大小的内存。
    // malloc() 成功时返回申请到的连续内存的首地址
    // 申请到的内存数据的值是不确定的，如果不清零就直接使用，里面可能会有一些内存段原本就带有数据，影响程序的运行；
    // here void -> casted as InputBuffer*
    InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    // 为了使用指向该结构的指针访问结构的成员，您必须使用 -> 运算符
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}

// print_prompt() prints a prompt to the user. 
// We do this before reading each line of input.
void print_prompt() { printf("db > "); }

// To read a line of input, use getline():

// lineptr : a pointer to the variable we use to point to the buffer 
// containing the read line. 
// If it set to NULL it is mallocatted by getline and 
// should thus be freed by the user, even if the command fails.

// n : a pointer to the variable we use to save the size of allocated buffer.

// stream : the input stream to read from. We’ll be reading from standard input.

// return value : the number of bytes read, 
// which may be less than the size of the buffer.

// We tell getline to store the read line in input_buffer->buffer 
// and the size of the allocated buffer in input_buffer->buffer_length. 
// We store the return value in input_buffer->input_length.
// buffer starts as null, so getline allocates enough memory to 
// hold the line of input and makes buffer point to it.
// lineptr：指向存放该行字符的指针，如果是NULL，则有系统帮助malloc，请在使用完成后free释放。
// stream：文件描述符
ssize_t getline(char **lineptr, size_t *n, FILE *stream);
// 多级指针，指向指针的指针, 例如，int ** 存 int * 的地址,  
// 二级指针存一级指针的地址，那么可以说二级指针指向一级指针

void read_input(InputBuffer* input_buffer){
    // stdin是标准输入，一般指键盘输入到缓冲区里的东西，采用perl语言实现。
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if (bytes_read <= 0) {
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }
    // Ignore trailing newline
    input_buffer->input_length = bytes_read-1;
    // 初始化由返回char数据的指针所组织成的数组，长度为bytes_read-1
    input_buffer->buffer[bytes_read-1] = 0;
}

// Now it is proper to define a function that frees the memory 
// allocated for an instance of InputBuffer * and 
// the buffer element of the respective structure 
// (getline allocates memory for input_buffer->buffer in read_input).
void close_input_buffer(InputBuffer* input_buffer){
    // 该函数一般与malloc配合使用，尽量每有一个malloc就要有一个free。
    // 它的作用是释放一块堆内存，其中ptr为堆内存的首地址，
    // 堆内存在使用完后要及时释放，否则这些内存就会被一直占用。
    // 注意free只是释放了使用权限，已经写入的数据不会全部清理，并且在free后应该及时置空ptr指针；
    free(input_buffer->buffer);
    free(input_buffer);
}

// a wrapper for existing functionality that leaves room for more commands
MetaCommandResult do_meta_command(InputBuffer* input_buffer){
    if (strcmp(input_buffer->buffer, ".exit") == 0){
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

// Our SQL Compiler
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){
    // strcmp与strncmp都是用来比较字符串的,区别在于strncmp是比较指定长度字符串,两者都是二进制安全的,且区分大小写。
    // use strncmp for "insert" since "insert" keyword will be followed by data
    if (strncmp(input_buffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, "select") == 0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

void execute_statement(Statement* statement){
    switch (statement->type){
        case (STATEMENT_INSERT):
            printf("do insertion");
            break;
        case (STATEMENT_SELECT):
            printf("do select");
            break;
    }
}

// argc 为参数个数，argv是字符串数组, 第一个存放的是可执行程序的文件名字，然后依次存放传入的参数
int main(int argc, char* argv[])
{
    // Sqlite starts a read-execute-print-loop when you start it from the command line
    // to do that, the main function will have
    // an infinite loop that prints the prompt, gets a line of input, 
    // then processes that line of input:
    InputBuffer* input_buffer = new_input_buffer();
    while (true) {
        print_prompt();
        read_input(input_buffer);
        // Deal with non-SQL statements like .exit
        // They are called meta-commands, starting with a dot
        if (input_buffer->buffer[0] == '.'){
            switch (do_meta_command(input_buffer)){
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                    continue;
            }
        }
        // Add a step that converts the line of input into our internal representation of a statement
        // Our hacky version of the sqlite front-end
        // the front end of sqlite is a SQL compiler that parses a string
        // and outputs an internal representation called bytecode.
        // The bytecode is passed to the virtual machine, which executes it.
        Statement statement;
        switch (prepare_statement(input_buffer, &statement)){
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'. \n", input_buffer->buffer);
                continue;
        }
        // pass the prepared statement to execute_statement
        // this function will eventually become our virtual machine.
        execute_statement(&statement);
        printf("Executed. \n");
    }
    
}