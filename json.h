#ifndef s7k_json_lib
#define s7k_json_lib

#include "memmanager.h"
#include "vector.h"

typedef enum  {
    STR,
    INT,
    FLOAT,
    ARRAY,
    CHILD
} json_object_types; 

struct json_child_struct {
    struct json_pair_struct* fields; // vector
    // struct json_child_struct* parent; // pointer
};
typedef struct json_child_struct json_child;

typedef union {
    char* str;
    int num;
    float dec;
    struct json_object_struct* array; //vector
    struct json_child_struct child;
} json_data;

struct json_object_struct {
    json_object_types type;
    json_data data;
};
typedef struct json_object_struct json_object;

struct json_pair_struct {
    char* key;
    struct json_object_struct value;
};
typedef struct json_pair_struct json_pair;






struct Token_struct {
    char *start;
    char *end;
    struct Token_struct* parent;
};
typedef struct Token_struct Token;

void init_json();

json_child read_json(FILE* fd);
void save_json(FILE* fd, json_child* child);

json_pair parse_pair(Token*);
json_object read_object(char*, size_t);
json_object *read_array(char*, size_t);
json_child read_child(char*, size_t);

void fprintchild(FILE *fd, json_child *child, size_t tabs);
void fprintarray(FILE *fd, json_object *array, size_t tabs);

#endif