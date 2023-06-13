// dario.santos  /  oscar.olveira
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <openssl/evp.h>
#include <pthread.h>

#include "options.h"
#include "queue.h"


#define MAX_PATH 1024
#define BLOCK_SIZE (10*1024*1024)
#define MAX_LINE_LENGTH (MAX_PATH * 2)
#define MAX_WAIT 10000

void *sum1(void *args);
void *sum2(void *args);
void *sum3(void *args);
void *check1(void *args);
void *check2(void *args);


struct file_md5 {
    char *file;
    unsigned char *hash;
    unsigned int hash_size;
};

struct thread_data1{
    queue in_q;
    char *dir;
};

struct thread_data2{
    int id;
    queue in_q;
    queue out_q;
    char *dir;
    pthread_mutex_t *m_create;
    struct file_md5 *md5;
    char *ent;
};

struct thread_data3{
    FILE *out;
    int dirname_len;
    queue out_q;
    struct file_md5 *md5;
};

struct thread_cdata1{
    queue in_q;
    char *dir;
    char *file;
};

struct thread_cdata2{
    queue in_q;
    struct file_md5 md5_file;
    struct file_md5 *md5_in;
    pthread_mutex_t *m_check;
};

void get_entries(char *dir, queue q);


void print_hash(struct file_md5 *md5) {
    for(int i = 0; i < md5->hash_size; i++) {
        printf("%02hhx", md5->hash[i]);
    }
}


void read_hash_file(char *file, char *dir, queue q) {
    FILE *fp;
    char line[MAX_LINE_LENGTH];
    char *file_name, *hash;
    int hash_len;

    if((fp = fopen(file, "r")) == NULL) {
        printf("Could not open %s : %s\n", file, strerror(errno));
        exit(0);
    }

    while(fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        char *field_break;
        struct file_md5 *md5 = malloc(sizeof(struct file_md5));

        if((field_break = strstr(line, ": ")) == NULL) {
            printf("Malformed md5 file\n");
            exit(0);
        }
        *field_break = '\0';

        file_name = line;
        hash      = field_break + 2;
        hash_len  = strlen(hash);

        md5->file      = malloc(strlen(file_name) + strlen(dir) + 2);
        sprintf(md5->file, "%s/%s", dir, file_name);
        md5->hash      = malloc(hash_len / 2);
        md5->hash_size = hash_len / 2;


        for(int i = 0; i < hash_len; i+=2)
            sscanf(hash + i, "%02hhx", &md5->hash[i / 2]);

        q_insert(q, md5);
    }

    fclose(fp);
}


void sum_file(struct file_md5 *md5) {
    EVP_MD_CTX *mdctx;
    int nbytes;
    FILE *fp;
    char *buf;

    if((fp = fopen(md5->file, "r")) == NULL) {
        printf("Could not open %s\n", md5->file);
        return;
    }

    buf = malloc(BLOCK_SIZE);
    const EVP_MD *md = EVP_get_digestbyname("md5");

    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex(mdctx, md, NULL);

    while((nbytes = fread(buf, 1, BLOCK_SIZE, fp)) >0)
        EVP_DigestUpdate(mdctx, buf, nbytes);

    md5->hash = malloc(EVP_MAX_MD_SIZE);
    EVP_DigestFinal_ex(mdctx, md5->hash, &md5->hash_size);

    EVP_MD_CTX_destroy(mdctx);
    free(buf);
    fclose(fp);
}


void recurse(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISDIR(st.st_mode))
        get_entries(entry, q);
}


void add_files(char *entry, void *arg) {
    queue q = * (queue *) arg;
    struct stat st;

    stat(entry, &st);

    if(S_ISREG(st.st_mode)){
        q_insert(q, strdup(entry));
    }
   
}


void walk_dir(char *dir, void (*action)(char *entry, void *arg), void *arg) {
    DIR *d;
    struct dirent *ent;
    char full_path[MAX_PATH];

    if((d = opendir(dir)) == NULL) {
        printf("Could not open dir %s\n", dir);
        return;
    }

    while((ent = readdir(d)) != NULL) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") ==0)
            continue;

        snprintf(full_path, MAX_PATH, "%s/%s", dir, ent->d_name);

        action(full_path, arg);
    }

    closedir(d);
}


void get_entries(char *dir, queue q) {
    walk_dir(dir, add_files, &q);
    walk_dir(dir, recurse, &q);
}


void check(struct options opt) {
    queue in_q;
    struct file_md5 *md5_in, md5_file;

    pthread_t *thread_check1;
    thread_check1 = malloc(sizeof(pthread_t));

    struct thread_cdata1 *thread_c1;
    thread_c1=malloc(sizeof(struct thread_cdata1));

    pthread_t *thread_check2;
    thread_check2=malloc(opt.num_threads * sizeof(pthread_t));
    
    struct thread_cdata2 *thread_c2;
    thread_c2=malloc(opt.num_threads * sizeof(struct thread_cdata2));
    
    pthread_mutex_t *m_check2;
    m_check2=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m_check2, NULL);

    in_q  = q_create(1);

    thread_c1->dir=opt.dir;
    thread_c1->in_q=in_q;
    thread_c1->file=opt.file;
    pthread_create(thread_check1, NULL, check1, thread_c1);

    for(int i=0; i<opt.num_threads;i++){
        thread_c2[i].in_q=in_q;
        thread_c2[i].md5_file=md5_file;
        thread_c2[i].md5_in=md5_in;
        thread_c2[i].m_check=m_check2;
        pthread_create(&thread_check2[i], NULL, check2, &thread_c2[i]);
    }

    pthread_join(*thread_check1,NULL);
    for(int i = 0 ; i<opt.num_threads ; i++){
        pthread_join(thread_check2[i], NULL);
    }

    q_destroy(in_q);
    pthread_mutex_destroy(m_check2);
    free(thread_c1);
    free(thread_check1);
    free(thread_c2);
    free(thread_check2);
    free(m_check2);

}


void *check1(void *args){
    struct thread_cdata1 *thread_c1 = args;
    read_hash_file(thread_c1->file, thread_c1->dir, thread_c1->in_q);
    activar_bool(thread_c1->in_q);
    return NULL;
}

void *check2(void *args){
    struct thread_cdata2 *thread_c2=args;
    while(q_elements(thread_c2->in_q)!=0 || !q_bool(thread_c2->in_q)){
        pthread_mutex_lock(thread_c2->m_check);
        thread_c2->md5_in = q_remove(thread_c2->in_q);
        if(thread_c2->md5_in==NULL){
            pthread_mutex_unlock(thread_c2->m_check);
            break;
        }

        thread_c2->md5_file.file = thread_c2->md5_in->file;

        sum_file(&thread_c2->md5_file);

        if(memcmp(thread_c2->md5_file.hash, thread_c2->md5_in->hash, thread_c2->md5_file.hash_size)!=0) {
            printf("File %s doesn't match.\nFound:    ", thread_c2->md5_file.file);
            print_hash(&thread_c2->md5_file);
            printf("\nExpected: ");
            print_hash(thread_c2->md5_in);
            printf("\n");
        }

        free(thread_c2->md5_file.hash);

        free(thread_c2->md5_in->file);
        free(thread_c2->md5_in->hash);
        free(thread_c2->md5_in);
        pthread_mutex_unlock(thread_c2->m_check);
    }

    return NULL;
}


void sum(struct options opt) {
    queue in_q, out_q;
    char *ent;
    FILE *out;
    struct file_md5 *md5;
    int dirname_len;

    pthread_t *thread_create1;
    thread_create1=malloc(sizeof(pthread_t));

    struct thread_data1 *thread_args1;
    thread_args1=malloc(sizeof(struct thread_data1));

    pthread_t *thread_create2;
    thread_create2=malloc(opt.num_threads * sizeof(pthread_t));

    struct thread_data2 *thread_args2;
    thread_args2=malloc(opt.num_threads * sizeof(struct thread_data2));

    pthread_mutex_t *m_create2;
    m_create2=malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m_create2,NULL);

    pthread_t *thread_create3;
    thread_create3=malloc(sizeof(pthread_t));

    struct thread_data3 *thread_args3;
    thread_args3=malloc(sizeof(struct thread_data3));

    in_q  = q_create(1);
    out_q = q_create(1);

    thread_args1->dir=opt.dir;
    thread_args1->in_q=in_q;
    pthread_create(thread_create1, NULL, sum1, thread_args1);

    for(int i=0; i<opt.num_threads;i++){
        thread_args2[i].dir=opt.dir;
        thread_args2[i].in_q=in_q;
        thread_args2[i].out_q=out_q;
        thread_args2[i].m_create=m_create2;
        thread_args2[i].md5=md5;
        thread_args2[i].ent=ent;
        pthread_create(&thread_create2[i], NULL, sum2, &thread_args2[i]);
    }

    if((out = fopen(opt.file, "w")) == NULL) {
        printf("Could not open output file\n");
        exit(0);
    }

    dirname_len = strlen(opt.dir) + 1; // length of dir + /

    thread_args3->dirname_len=dirname_len;
    thread_args3->out=out;
    thread_args3->out_q=out_q;
    thread_args3->md5=md5;
    pthread_create(thread_create3, NULL, sum3, thread_args3);

    pthread_join(*thread_create1, NULL);
    for(int i=0; i<opt.num_threads; i++){
        pthread_join(thread_create2[i], NULL);
    }
    pthread_join(*thread_create3, NULL);

    fclose(out);
    q_destroy(in_q);
    q_destroy(out_q);
    pthread_mutex_destroy(m_create2);
    free(thread_create1);
    free(thread_args1);
    free(thread_create2);
    free(thread_args2);
    free(thread_create3);
    free(thread_args3);
    free(m_create2);
}


void *sum1(void *args){
    struct thread_data1* thread_args1=args;
    get_entries(thread_args1->dir, thread_args1->in_q);
    activar_bool(thread_args1->in_q);
    return NULL;
}


void *sum2(void *args){
    struct thread_data2* thread_args2=args;
    while(q_elements(thread_args2->in_q)!=0 || !q_bool(thread_args2->in_q)){
        pthread_mutex_lock(thread_args2->m_create);
        thread_args2->ent = q_remove(thread_args2->in_q);
        if(thread_args2->ent==NULL){
            pthread_mutex_unlock(thread_args2->m_create);
            break;
        }
        thread_args2->md5 = malloc(sizeof(struct file_md5));

        thread_args2->md5->file =thread_args2->ent;
        sum_file(thread_args2->md5);

        q_insert(thread_args2->out_q, thread_args2->md5);
        pthread_mutex_unlock(thread_args2->m_create);
        usleep(rand() % MAX_WAIT);
    }
    activar_bool(thread_args2->out_q);

    return NULL;
}


void *sum3(void *args){
    struct thread_data3* thread_args3=args;
    while(q_elements(thread_args3->out_q)!=0 || !q_bool(thread_args3->out_q)) {
        thread_args3->md5=q_remove(thread_args3->out_q);
        if(thread_args3->md5==NULL){
            break;
        }
        fprintf(thread_args3->out, "%s: ", thread_args3->md5->file + thread_args3->dirname_len);

        for(int i = 0; i < thread_args3->md5->hash_size; i++)
            fprintf(thread_args3->out, "%02hhx", thread_args3->md5->hash[i]);
        fprintf(thread_args3->out, "\n");

        free(thread_args3->md5->file);
        free(thread_args3->md5->hash);
        free(thread_args3->md5);
    }    

    return NULL;
}


int main(int argc, char *argv[]) {

    struct options opt;

    opt.num_threads = 5;
    opt.queue_size  = 1000;
    opt.check       = true;
    opt.file        = NULL;
    opt.dir         = NULL;

    read_options (argc, argv, &opt);

    if(opt.check)
        check(opt);
    else
        sum(opt);

}