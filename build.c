#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#include <dirent.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#include "json.h"

json_child handler;
char *indir, *outdir, *compiler, *format, *libs, *cflags, *target;
char** cpp_source; // vectors

struct stat _lasttime;
time_t lastUpdateTime(const char* filename) {
    if(stat(filename, &_lasttime)==0) {
        return _lasttime.st_mtime;
    }
    return time(NULL);
}

bool file_exists (const char* name) {
    FILE* file;
    if (file = fopen(name, "r")) {
        fclose(file);
        return true;
    }
    return false;   
}

bool error(const char* message) {
    fprintf(stdout, message);
    destroy_pages();
    return false;
}

bool dir_exists(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}

bool cmd_exec(const char* command) {
    // printf(command);
    int result = system(command);
    return result==0;
}

bool in_vector(char* string, char** vector, size_t size) {
    for (int i=0; i<size; i++) {
        if (strcmp(string, vector[i])==0) {
            return true;
        }
    }
    return false;
}

void print_info_about() {
    printf("-b \t builds the binaries\n--build\n\n"
    "-fb \t forcefully recompiles to obj files, even if already exists, then builds the binaries\n--force_build\n\n"
    "-r \t recompiles obj files\n--recompile\n\n"
    "-fr \t forcefully recompiles to obj files, even if already exists\n--force_recompile\n\n");
}

bool spec_recompile(char* filename) {
    char buff[256];
    sprintf(buff, "%s %s %s%s%s -c -o %s%s.o", compiler, cflags, indir, filename, format, outdir, filename);
    return cmd_exec(buff);
}

bool recompile(bool force) {
    if (!dir_exists(indir)) return error("Indir does not exist\n");

    
    if (!dir_exists(outdir)) {
        mkdir(outdir);
    }
    printf("Compilation:\n");
    vector_metainfo meta;
    meta = vec_meta(cpp_source);
    char buff[256];
    time_t t1, t2;
    struct timespec start, stop;
    // printf("%d", meta.length);
    for (int i=0; i<meta.length; i++) {
        sprintf(buff, "%s%s.o", outdir, cpp_source[i]);
        if (file_exists(buff)) {
            t1 = lastUpdateTime(buff);
            // printf(buff);
            sprintf(buff, "%s%s%s", indir, cpp_source[i], format);
            t2 = lastUpdateTime(buff);
            // printf(buff);
            if (t1 >= t2) {
                if (!force) continue;
            }
        }
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (!spec_recompile(cpp_source[i])) {
            sprintf(buff, "Cannot compile file %s%s\n", cpp_source[i], format);
            return error(buff);
        }
        clock_gettime(CLOCK_MONOTONIC, &stop);
        printf("\t\033[34m %s%s:\033[0m\t %lu ms\n", cpp_source[i], format, (stop.tv_nsec-start.tv_nsec)/1000000);
    }

    return true;
}

bool build(bool force) {
    if (!recompile(force)) return error("Compilation error\n");
    
    char buff[512]; *buff = '\0';
    int written = 0;
    sprintf(buff+written, "%s ", compiler);
    written = strlen(buff);
    vector_metainfo meta = vec_meta(cpp_source);
    
    for (int i=0; i<meta.length; i++) {
        sprintf(buff+written, "%s%s.o ", outdir, cpp_source[i]);
        written = strlen(buff);
    }

    sprintf(buff+written, libs);
    written = strlen(buff);

    sprintf(buff+written, " -o %s%s", outdir, target);
    
    printf("Linking:\n");
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    bool result = cmd_exec(buff);
    if (result) {
        clock_gettime(CLOCK_MONOTONIC, &stop);
        printf("\t%lu ms\n", (stop.tv_nsec-start.tv_nsec)/1000000);
    }
    return result;
}

bool load_build_data() {
    FILE* fd = fopen("build.json", "r");
    handler = read_json(fd);
    fclose(fd);
    vector_metainfo meta = vec_meta(handler.fields);
    json_pair temp;
    json_object obj;

    indir = NULL;
    outdir = NULL;
    compiler = NULL; 
    format = NULL; 
    libs = NULL; 
    cflags = NULL; 
    target = NULL;
    cpp_source = NULL;
    
    vector_metainfo mt;
    for (int i=0; i<meta.length; i++) {
        temp = handler.fields[i];
        obj = temp.value;
        if (strcmp(temp.key, "target")==0) {
            target = obj.data.str;
        } else if (strcmp(temp.key, "indir")==0) {
            indir = obj.data.str;
        } else if (strcmp(temp.key, "outdir")==0) {
            outdir = obj.data.str;
        } else if (strcmp(temp.key, "compiler")==0) {
            compiler = obj.data.str;
        } else if (strcmp(temp.key, "format")==0) {
            format = obj.data.str;
        } else if (strcmp(temp.key, "cflags")==0) {
            cflags = obj.data.str;
        } else if (strcmp(temp.key, "libs")==0) {
            libs = obj.data.str;
        } else if (strcmp(temp.key, "cpp_source")==0) {
            mt = vec_meta(obj.data.array);
            cpp_source = new_vec(sizeof(char*), mt.length);
            for (int j=0; j<mt.length; j++) {
                cpp_source = vec_add(cpp_source, &(obj.data.array[j].data.str));
                // printf("%s\n", cpp_source[j]);
            }
        }
    }
    // printf("%s %s %s %s %s %s %s\n", indir, outdir, compiler, format, libs, cflags, target);
    return indir && outdir && compiler && format && libs && cflags && target && cpp_source;
}



int main(int argc, char** argv) {
    init_json();
    system("");
    if (!load_build_data()) {
        printf("\033[31;1m Cannot read json-file \033[0m\n");
        return 1;
    }

    bool result = true;
    struct timespec start, stop;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (argc > 1) {
        if (in_vector("--help", argv, argc)) {
            print_info_about();
            return 0;
        }

        if (in_vector("-fb", argv, argc) || in_vector("-bf", argv, argc) || in_vector("--force_build", argv, argc)) {
            result = build(true);
        }
        else if (in_vector("-fr", argv, argc) || in_vector("-rf", argv, argc) || in_vector("--force_recompile", argv, argc)) {
            result = recompile(true);
        }
        else if (in_vector("-b", argv, argc) || in_vector("--build", argv, argc)) {
            result = build(false);
        }
        else if (in_vector("-r", argv, argc) || in_vector("--recompile", argv, argc)) {
            result = recompile(false);
        }
    } else {
        result = build(false);
        //print_info_about();
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    
    if (!result) {
        printf("\033[31;1m Error occurred \033[0m\n");
    } else {
        printf("\033[32m Finished\033[0m total in %lu ms\n", (stop.tv_nsec-start.tv_nsec)/1000000);
    }

    return result;
}