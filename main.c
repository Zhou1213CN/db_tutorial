#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// PART 3 An In-Memory, Append-Only, Single-Table Database
// For now, it will 
// support two operations: inserting a row and print all rows
// reside only in memory (no persistence to disk)
// support a single, hard-coded table

// Our hard-coded table is going to store users and look like this:
// column	type
// id	integer
// username	varchar(32)
// email	varchar(255)

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
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
} ExecuteResult;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum { PREPARE_SUCCESS, PREPARE_SYNTAX_ERROR, PREPARE_UNRECOGNIZED_STATEMENT } PrepareResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct {
    StatementType type;
    Row row_to_insert; // only used by insert statement
} Statement;

// We need to copy data into some data structure representing the table
// Like a B-tree, it will group rows into pages and arrange pages as an array.
// Plan
// Store rows in blocks of memory called pages
// Each page stores as many rows as it can fit
// Rows are serialized into a compact representation with each page
// Pages are only allocated as needed
// Keep a fixed-size array of pointers to pages

// 这个宏函数的作用是获取struct中属性的长度
// 0 被强制转换为结构类型指针，并且那么该指针用于访问成员变量
// 用作sizeof运算符的操作数。
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

// This means the layout of a serialized row will look like this:
// column	size (bytes)	offset
// id	4	0
// username	32	4
// email	255	36
// total	291	 

// Next, a Table structure that points to pages of rows and keeps track of how many rows there are:
const uint32_t PAGE_SIZE = 4096; // 4 kilobytes, the same size as a page used in the virtual memory systems of most computer architectures
// one page in our database corresponds to one page used by the os
// os will move pages in and out of memory as whole units instead of breaking them up
#define TABLE_MAX_PAGES 100 //arbitrary
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
} Table;

void print_row(Row* row) {
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void serialize_row(Row* source, void* destination){
    // C 库函数 void *memcpy(void *str1, const void *str2, size_t n) 从存储区 str2 复制 n 个字节到存储区 str1。
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination){
    // C 库函数 void *memcpy(void *str1, const void *str2, size_t n) 从存储区 str2 复制 n 个字节到存储区 str1。
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

// read/write in memory for a particular row
void* row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = table->pages[page_num];
    if (page == NULL){
        // Allocate memory only when we try to access page
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;
}

// Initialize the table, create the respective memory release function and handle a few more error cases:
Table* new_table(){
    Table* table = (Table*)malloc(sizeof(Table));
    table->num_rows = 0;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++){
        table->pages[i] = NULL;
    }
    return table;
}

void free_table(Table* table){
    for (int i = 0; table->pages[i]; i++) {
        free(table->pages[i]);
    }
    free(table);
}


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

    // insert statements are now going to look like this:
    // insert 1 cstack foo@bar.com
    if (strncmp(input_buffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        // sscanf从字符串读取格式化输入
        // 把 sscanf 操作的结果存储在一个普通的变量中，应该在标识符前放置引用运算符（&）
        // 如果成功，sscanf 返回成功匹配和赋值的个数。
        int args_assigned = sscanf(input_buffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id),
        statement->row_to_insert.username, statement->row_to_insert.email);
        if (args_assigned < 3) {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, "select") == 0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

// make execute_statement read/write from our tables structure
ExecuteResult execute_insert(Statement* statement, Table* table){
    if (table->num_rows >= TABLE_MAX_ROWS){
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, row_slot(table, table->num_rows));
    table->num_rows += 1;

    return EXIT_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table){
    Row row;
    for (uint32_t i = 0; i < table->num_rows;i++){
        deserialize_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXIT_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table){
    switch (statement->type){
        case (STATEMENT_INSERT):
            return execute_insert(statement, table);
        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}

// argc 为参数个数，argv是字符串数组, 第一个存放的是可执行程序的文件名字，然后依次存放传入的参数
int main(int argc, char* argv[])
{
    // Sqlite starts a read-execute-print-loop when you start it from the command line
    // to do that, the main function will have
    // an infinite loop that prints the prompt, gets a line of input, 
    // then processes that line of input:
    Table* table = new_table();
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
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement. \n");
                continue;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'. \n", input_buffer->buffer);
                continue;
        }
        // pass the prepared statement to execute_statement
        // this function will eventually become our virtual machine.
        switch (execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full. \n");
                break;
        }
        
    }
    
}