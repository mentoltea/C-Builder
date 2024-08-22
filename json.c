#include "json.h"

void init_json() {
    set_funcs(memloc, memfree, memcpy);
    prealloc(PAGE_SIZE);
}

void exit_on_error(int code) {
    page_info(0);
    destroy_pages();
    exit(code);
}

// reads the entire file
char* read_from_file(FILE* fd, size_t size) {
    char *str = memloc(size);
    fread(str, sizeof(char), size, fd);
    return str;
}

char* string_from_ptrs(char* start, char* end) {
    char *str = memloc(sizeof(char)*(end-start+1));
    memcpy(str, start, sizeof(char)*(end-start+1));
    str[end-start] = '\0';
    return str;
}

char* remove_spaces(char* str, size_t size) {
    while (size!=0 && (*str <=32)) {
        str++; size--;
    }
    while (size!=0 && (*(str+size-1) <= 32)) {
        size--;
    }
    char* res;
    if (size==0) return NULL;
    res = string_from_ptrs(str, str+size);
    return res;
}

size_t count_of(char *start, char *end, char symb) {
    char* ptr = start;
    size_t size = 0;
    while (ptr && ptr<end) {
        ptr = strchr(ptr, symb);
        if (!ptr || ptr>=end) break;
        size++;
        ptr++;
    }
    return size;
}

// splits everything into pairs
// @return vector of tokens
Token* split_into_pairs(char *str, size_t size, int array) {
    char *ptr, *start;
    char *end = str+size;
    char stopsymb = '}';
    char symsymb = '{';
    if (array) {
        stopsymb = ']';
        symsymb = '[';
    }
    while (size!=0 && *end != stopsymb) {
        end--; size--;
    }
    if (size==0) {
        printf("Not found global %c\n", stopsymb);
        exit_on_error(1);
    }
    
    Token* vec = new_vec(sizeof(Token), 10);
    Token temp;

    // printf("%s|\n\n", string_from_ptrs(str, end));
    // printf("|%c %c|\n", *str, *end);
    for (ptr = str+1; ptr < end; ptr++) {
        if (*ptr == ',') {
            ptr++;
        }
        if (*ptr=='\n' || *ptr=='\t') {
            continue;
        }

        start = ptr;
        ptr = strchr(ptr, ',');
        if (!ptr) ptr = end;

        if (*ptr == '\0') ptr--;
        // if (ptr==end) ptr--;
        temp.start = start; temp.end = ptr; temp.parent = NULL;
        // printf("<%c %c>\n", *start, *ptr);
        vec = vec_add(vec, &temp);
    }
    vector_metainfo *meta = vec_metaptr(vec);
    Token* pairs = new_vec(sizeof(Token), 10);
    
    for (int i=0; i<meta->length; i++) {
        temp = vec[i];
        // printf("%s\n\n", string_from_ptrs(temp.start, temp.end));
        if (count_of(temp.start, temp.end, '"')%2 == 1) {
            if (i!=meta->length-1) {
                vec[i+1].start = temp.start;
                continue;
            }
            printf("Syntax error with \"\n");
            printf("%s\n", string_from_ptrs(temp.start, temp.end));
            exit_on_error(1);
        }
        if (count_of(temp.start, temp.end, '[') != count_of(temp.start, temp.end, ']')) {
            if (i!=meta->length-1) {
                vec[i+1].start = temp.start;
                continue;
            }
            printf("Syntax error with []\n");
            // printf("%s\n", string_from_ptrs(vec[i-1].start, vec[i-1].end));
            printf("%s\n", string_from_ptrs(temp.start, temp.end));
            exit_on_error(1);
        } 
        if (count_of(temp.start, temp.end, '{') != count_of(temp.start, temp.end, '}')) {
            if (i!=meta->length-1) {
                vec[i+1].start = temp.start;
                continue;
            }
            printf("Syntax error with {}\n");
            printf("%s\n", string_from_ptrs(temp.start, temp.end));
            exit_on_error(1);
        }
        pairs = vec_add(pairs, &temp);
    }
    delete_vec(vec);
    return pairs;
}

json_pair parse_pair(Token *tk) {
    json_pair pair;
    char* ptr = strchr(tk->start, ':');
    if (!ptr || ptr>=tk->end) {
        printf("Cannot parse pair\n%s\n", string_from_ptrs(tk->start, tk->end));
        exit_on_error(1);
    }
    char* key = remove_spaces(tk->start+1, ptr-tk->start-1);
    char* value = remove_spaces(ptr+1, tk->end-ptr-1);
    size_t keysize = strlen(key);
    if (*key=='"') {
        key++; keysize--;
    }
    if (*(key+keysize-1)=='"'){
        key[keysize-1] = '\0';
    }
    pair.key = key;
    // printf("%s\n", value);
    //printf("SIZE OF %s %d\n", value, strlen(value));
    pair.value = read_object(value, strlen(value)+1);
    memfree(value);
    return pair;   
}

json_object_types object_type(char* value, size_t size) {
    json_object_types type;
    switch (*value) {
        case '[':
            type = ARRAY;
            break;
        case '{':
            type = CHILD;
            break;
        case '"':
            type = STR;
            break;
        default:
            type = INT;
            break;
    }
    char* ptr;
    if (type == INT) {
        ptr = strchr(value, '.');
        if (ptr && ptr<value+size) {
            type = FLOAT;
        }
    }

    return type;
}

json_object read_object(char* value, size_t size) {
    // printf("%s\n", value);
    json_object_types type = object_type(value, size);
    json_object obj; obj.type = type;
    // json_child *child;
    switch (type) {
        case STR:
            obj.data.str = string_from_ptrs(value+1, value+size-1);
            int s = strlen(obj.data.str);
            if (obj.data.str[s-1]=='"') obj.data.str[s-1]='\0';
            printf("%s | %s\n", value, obj.data.str);
            break;
        case INT:
            obj.data.num = strtol(value, NULL, 0);
            // printf("num | %d\n", obj.data.num);
            break;
        case FLOAT:
            obj.data.dec = strtof(value, NULL);
            // printf("dec | %f\n", obj.data.dec);
            break;
        case ARRAY:
            obj.data.array = read_array(value, size);
            // printf("array | %s\n", value);
            break;
        case CHILD:
            // printf("child | %s\n", value);
            // child = memloc(sizeof(json_child));
            // *child = read_child(value, size);
            obj.data.child = read_child(value, size);
            break;
        default:
            break;
    }
    return obj;
}

// @return vector of objects
json_object *read_array(char *value, size_t size) {
    // printf("%s\n\n", string_from_ptrs(value, value+size));
    Token* pairs = split_into_pairs(value, size, 1); // vector
    vector_metainfo meta = vec_meta(pairs);
    json_object *array = new_vec(sizeof(json_object), meta.length);
    json_object temp;
    for (int i=0; i<meta.length; i++) {
        // printf(remove_spaces(pairs[i].start, pairs[i].end - pairs[i].start));
        // printf("##\n");
        temp = read_object(remove_spaces(pairs[i].start, pairs[i].end - pairs[i].start),
                pairs[i].end - pairs[i].start);
        // printf("%d\n", temp.type);
        array = vec_add(array, &temp);
    }
    delete_vec(pairs);
    return array;
}

json_child read_child(char *str, size_t size) {
    Token* pairs = split_into_pairs(str, size+1, 0); // vector
    vector_metainfo meta = vec_meta(pairs);
    json_child child;
    child.fields = new_vec(sizeof(json_pair), meta.length);
    json_pair temp;
    for (int i=0; i<meta.length; i++) {
        temp = parse_pair(pairs+i);
        child.fields = vec_add(child.fields, &temp);
    }
    delete_vec(pairs);
    return child;
}

json_child read_json(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    char* str = read_from_file(fd, size);
    json_child result = read_child(str, size);
    memfree(str);
    return result;
}


void fprintarray(FILE *fd, json_object *array, size_t tabs) {
    size_t iter;

    // iter = tabs;
    // while (iter>0) {
    //     printf("\t");
    //     iter--;
    // } 
    fprintf(fd, "[\n");

    // tabs++;
    vector_metainfo meta = vec_meta(array);
    // printf("size %d\n", meta.length);
    json_object temp;
    // for (int i=0; i<meta.length; i++) {
    //     printf("%d ", array[i].type);
    // }
    for (int i=0; i<meta.length; i++) {
        temp = array[i];
        iter = tabs;
        while (iter>0) {
            fprintf(fd, "\t");
            iter--;
        } 
        switch (temp.type) {
            case STR:
                fprintf(fd, "\"%s\"", temp.data.str);
                break;
            case INT:
                fprintf(fd, "%d", temp.data.num);
                break;
            case FLOAT:
                fprintf(fd, "%f", temp.data.dec);
                break;
            case CHILD:
                fprintchild(fd, &temp.data.child, tabs);
                break;
            case ARRAY:
                fprintarray(fd, temp.data.array, tabs+1);
                break;

            default:
                break;
        }
        if (i!=meta.length-1) {
            fprintf(fd, ",");
        }
        //printf(" | %d", temp.type);
        fprintf(fd, "\n");
    }
    tabs--;
    iter = tabs;
    while (iter>0) {
        fprintf(fd, "\t");
        iter--;
    } fprintf(fd, "]");
}

void fprintchild(FILE *fd, json_child *child, size_t tabs) {
    size_t iter;

    // iter = tabs;
    // while (iter>0) {
    //     printf("\t");
    //     iter--;
    // }  
    fprintf(fd, "{\n");

    tabs++;
    vector_metainfo meta = vec_meta(child->fields);
    // printf("size %d\n", meta.length);
    json_pair temp;
    for (int i=0; i<meta.length; i++) {
        temp = child->fields[i];
        iter = tabs;
        while (iter>0) {
            fprintf(fd, "\t");
            iter--;
        } 
        fprintf(fd, "\"%s\" : ", temp.key);
        switch (temp.value.type) {
            case STR:
                fprintf(fd, "\"%s\"", temp.value.data.str);
                break;
            case INT:
                fprintf(fd, "%d", temp.value.data.num);
                break;
            case FLOAT:
                fprintf(fd, "%f", temp.value.data.dec);
                break;
            case CHILD:
                fprintchild(fd, &temp.value.data.child, tabs);
                break;
            case ARRAY:
                fprintarray(fd, temp.value.data.array, tabs+1);
                break;

            default:
                break;
        }
        if (i!=meta.length-1) {
            fprintf(fd, ",");
        }
        //printf(" | %d", temp.value.type);
        fprintf(fd, "\n");
    }
    tabs--;
    iter = tabs;
    while (iter>0) {
        fprintf(fd, "\t");
        iter--;
    } fprintf(fd, "}");
    if (tabs == 0) {
        fprintf(fd, "\n");
    }
}

void save_json(FILE* fd, json_child* child) {
    fprintchild(fd, child, 0);
}