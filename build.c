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

typedef struct cpp_file {
    bool linkable;
    char* compiler;
    char* name;
    char* format;
    char* cflags;
    char* libs;
    char* target;
    char** dependencies; // vector
} cpp_file;


json_child handler;
char *indir, *outdir, *targetdir, *compiler, *linker, *format, *libs, *cflags, *target;
cpp_file* cpp_source; // vectors

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
    fprintf(stderr, message);
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
    // printf("%s\n", command);
    // return 1;
    int result = system(command);
    return result==0;
}

bool makedir(const char* filename) {
#if defined(WIN32)
    // printf("Windows\n");
    return mkdir(filename)==0;
#elif defined(__linux__)||defined(__unix__)
    // printf("Linux\n");
    return mkdir(filename, S_IFDIR)==0;
#else
    printf("\033[33;1mNot found system. Using terminal mkdir to create directory\033[0m\n");
    char buff[64];
    sprintf(buff, "mkdir %s", filename);
    return cmd_exec(buff);
#endif
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

bool spec_recompile(cpp_file* file) {
    char buff[256];
    size_t cursor = 0;
    sprintf(buff+cursor, "%s %s %s%s%s ", file->compiler, file->cflags, indir, file->name, file->format);
    cursor = strlen(buff);

    if (file->libs) {
        sprintf(buff+cursor, "%s ", file->libs);
        cursor = strlen(buff);
    }

    if (file->target) sprintf(buff+cursor, "-o %s%s ", outdir, file->target);
    else sprintf(buff+cursor, "-o %s%s.o ", outdir, file->name);
    cursor = strlen(buff);

    return cmd_exec(buff);
}

bool recompile(bool force) {
    if (!dir_exists(indir)) return error("Indir does not exist\n");

    
    if (!dir_exists(outdir)) {
        if (!makedir(outdir)) {
            return error("Cannot create output directory\n");
        }
    }
    printf("\033[33mCompilation:\033[0m\n");
    vector_metainfo meta = vec_meta(cpp_source);
    cpp_file *file;
    char buff[64];
    time_t t1, t2;
    struct timespec start, stop;
    // printf("%d", meta.length);
    for (int i=0; i<meta.length; i++) {
        file = cpp_source+i;
        
        if (file->target) sprintf(buff, "%s%s", outdir, file->target);
        else sprintf(buff, "%s%s.o", outdir, file->name);

        if (!force) {    
            if (file_exists(buff)) {
                t1 = lastUpdateTime(buff);
                sprintf(buff, "%s%s%s", indir, file->name, file->format);
                t2 = lastUpdateTime(buff);
                if (t1 >= t2) {
                    bool flag = true;
                    if (file->dependencies) {
                        vector_metainfo dep_mt = vec_meta(file->dependencies);
                        for (int j=0; j<dep_mt.length; j++) {
                            t2 = lastUpdateTime(file->dependencies[j]);
                            if (t2 > t1) {
                                printf("\tNoticed change in dependence \033[34m%s\033[0m\n", file->dependencies[j]);
                                flag=false; 
                                break;
                            }
                        }
                    }
                    if (flag) continue;
                }
            }
        }

        clock_gettime(CLOCK_REALTIME, &start);
        if (!spec_recompile(file)) {
            sprintf(buff, "\tCannot compile file %s%s%s\n", indir, file->name, file->format);
            return error(buff);
        }
        clock_gettime(CLOCK_REALTIME, &stop);

        printf("\t\033[34m %s%s\033[0m -> ", file->name, file->format); 
        if (file->target) printf("\033[34m%s:\033[0m", file->target);
        else printf("\033[34m%s.o:\033[0m", file->name);
        printf("\t %ld ms\n", abs((stop.tv_nsec-start.tv_nsec)/1000000));
    }

    return true;
}

bool build(bool force) {
    if (!recompile(force)) return error("Compilation error\n");
    
    char buff[256]; *buff = '\0';
    size_t written = 0;
    snprintf(buff+written, 256-written, "%s ", linker);
    written = strlen(buff);
    vector_metainfo meta = vec_meta(cpp_source);
    cpp_file *file;
    
    for (int i=0; i<meta.length; i++) {
        file = cpp_source + i;
        if (!file->linkable) continue;
        if (file->target) snprintf(buff+written, 256-written, "%s%s ", outdir, file->target);
        else snprintf(buff+written, 256-written, "%s%s.o ", outdir, file->name);
        written = strlen(buff);
    }

    if (libs) {
        snprintf(buff+written, 256-written, "%s ", libs);
        written = strlen(buff);
    }

    snprintf(buff+written, 256-written, " -o %s%s", targetdir, target);
    
    printf("\033[33mLinking:\033[0m\n");
    struct timespec start, stop;
    clock_gettime(CLOCK_REALTIME, &start);
    // printf("FIN LEN: %d\n", strlen(buff));
    bool result = cmd_exec(buff);
    if (result) {
        clock_gettime(CLOCK_REALTIME, &stop);
        printf("\t%ld ms\n", abs((stop.tv_nsec-start.tv_nsec)/1000000));
        // printf("In build\n");
    }
    // printf("In build2\n");
    return result;
}

bool load_build_data(FILE* fd) {
    handler = read_json(fd);
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
        } else if (strcmp(temp.key, "targetdir")==0) {
            targetdir = obj.data.str;
        } else if (strcmp(temp.key, "compiler")==0) {
            compiler = obj.data.str;
        } else if (strcmp(temp.key, "linker")==0) {
            linker = obj.data.str;
        } else if (strcmp(temp.key, "format")==0) {
            format = obj.data.str;
        } else if (strcmp(temp.key, "cflags")==0) {
            cflags = obj.data.str;
        } else if (strcmp(temp.key, "libs")==0) {
            libs = obj.data.str;
        } else if (strcmp(temp.key, "cpp_source")==0) {
            mt = vec_meta(obj.data.array);
            cpp_source = new_vec(sizeof(cpp_file), mt.length);
            for (int j=0; j<mt.length; j++) {
                cpp_file file = {0};
                file.linkable = 1;
                json_object inner = obj.data.array[j];
                if (inner.type == CHILD) {
                    vector_metainfo fields_inner = vec_meta(inner.data.child.fields);
                    for (int m = 0; m < fields_inner.length; m++) {
                        json_pair inn_pair = inner.data.child.fields[m];
                        json_object inobj = inn_pair.value;
                        if (strcmp(inn_pair.key, "target")==0) {
                            file.target = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "format")==0) {
                            file.format = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "name")==0) {
                            file.name = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "compiler")==0) {
                            file.compiler = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "cflags")==0) {
                            file.cflags = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "dependencies")==0) {
                            vector_metainfo dep_mt = vec_meta(inobj.data.array);
                            file.dependencies = new_vec(sizeof(char*), dep_mt.length);
                            for (int d=0; d < dep_mt.length; d++) {
                                file.dependencies = vec_add(file.dependencies, &(inobj.data.array[d].data.str));
                            }
                        } else if (strcmp(inn_pair.key, "libs")==0) {
                            file.libs = inobj.data.str;
                        } else if (strcmp(inn_pair.key, "linkable")==0) {
                            file.linkable = inobj.data.num;
                        }
                    }
                } else if (inner.type == STR) {
                    file.name = inner.data.str;
                } else {
                    return error("Inapropriate type of cpp source file\n");
                }

                
                if (!file.name) {
                    return error("File name is not provided\n");
                }
                if (!file.cflags) 
                    if (cflags && strlen(cflags)>0) file.cflags = cflags;
                    else file.cflags = "-c";
                if (!file.libs)
                    if (libs && strlen(libs)>0) file.libs = libs;
                if (!file.compiler) file.compiler = compiler;
                if (!file.format) file.format = format;

                cpp_source = vec_add(cpp_source, &file);

                // printf("%s %s %s %s %s %s %d\n", file.name, file.format, file.compiler,  file.target, file.cflags, file.libs, file.linkable);
                // if (file.dependencies) {
                //     vector_metainfo dep_mt = vec_meta(file.dependencies);
                //     for (int d=0; d < dep_mt.length; d++) {
                //         printf("%s ", file.dependencies[d]);
                //     }
                //     printf("\n");
                // }
            }

        }
    }
    // printf("%s %s %s %s %s %s %s\n", indir, outdir, compiler, format, libs, cflags, target);
    return indir && outdir && targetdir && compiler && format && target && cpp_source;
}

typedef enum Todo {
    UNKNOWN,
    RECOMPILE,
    BUILD
} Todo;

typedef enum FlagForce {
    NOTFORCE = 0,
    FORCE
} FlagForce;

int main(int argc, char** argv) {
    system("");
    printf("\033[36;1m C-Builder by s7k \n\033[0m");
    
    bool result = false;
    Todo todo;
    FlagForce flagforce;
    struct timespec start, stop;
    char* filename = NULL;
    
    
    if (in_vector("--help", argv, argc)) {
        print_info_about();
    } else if (in_vector("-fb", argv, argc) || in_vector("-bf", argv, argc) || in_vector("--force_build", argv, argc)) {
        todo = BUILD;
        flagforce = FORCE;
    }
    else if (in_vector("-fr", argv, argc) || in_vector("-rf", argv, argc) || in_vector("--force_recompile", argv, argc)) {
        todo = RECOMPILE;
        flagforce = FORCE;
    }
    else if (in_vector("-b", argv, argc) || in_vector("--build", argv, argc)) {
        todo = BUILD;
        flagforce = NOTFORCE;
    }
    else if (in_vector("-r", argv, argc) || in_vector("--recompile", argv, argc)) {
        todo = BUILD;
        flagforce = NOTFORCE;
    }
    else {
        todo = BUILD;
        flagforce = NOTFORCE;
    }

    for (int i=1; i<argc; i++) {
        if (argv[i][0]!='-') {
            filename = argv[i];
            break;
        }
    }
    if (!filename) filename = "build.json";

    FILE* fd = fopen(filename, "r");
    if (!fd) {
        printf("\033[31;1m Cannot open file \033[0m\n");
        goto EXIT_BUILDER;
    }
    init_json();
    if (!load_build_data(fd)) {
        printf("\033[31;1m Cannot read json-file \033[0m\n");
        fclose(fd);
        goto EXIT_BUILDER;
    }
    printf("\033[36m JSON-file succesfully read \n\033[0m");
    fclose(fd);


    clock_gettime(CLOCK_REALTIME, &start);
    switch (todo)
    {
    case RECOMPILE:
        result = recompile(flagforce);
        break;

    case BUILD:
        result = build(flagforce);
        break;
    
    default:
        result = 1;
        printf("\033[31;1m Cannot understand settings \033[0m\n");
        break;
    }
    clock_gettime(CLOCK_REALTIME, &stop);

EXIT_BUILDER:
    destroy_pages();
    // printf("\033[36m Pages deallocated \n\033[0m");

    if (!result) {
        printf("\033[31;1m Error occurred \033[0m\n");
    } else {
        printf("\033[32m Finished\033[0m total in %ld ms\n", abs((stop.tv_nsec-start.tv_nsec)/1000000));
    }

    return result;
}