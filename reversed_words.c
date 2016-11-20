#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define TRUE (1)
#define FALSE (0)

/* Maximum length of word including null terminator */
#define WORDSIZE (128)

#ifndef USE_THREADS
#define USE_THREADS (0)
#endif

#if USE_THREADS
#define MAXWORKERS (10)
pthread_mutex_t lock;
#endif

FILE *output;

unsigned char *file_beginning;
unsigned char *file_end;

unsigned char *file_offset;

int   read_word();
void  reverse_word(char*, char*, int);
int   print_to_file(char*, char*);
void* next_word(char*, int);
int   word_exists(char*, int);
void* find_reversed_words(void*);

int main(int argc, const char *argv[])
{
    char *filename_in = "words";
    char *filename_out = "out";
    int file_len;
    struct stat buf;
    struct timeval time1, time2;

#if USE_THREADS
    long i;
    pthread_t workerid[MAXWORKERS];
    pthread_mutex_init(&lock, NULL);
    int numWorkers = (argc > 1)? atoi(argv[1]) : MAXWORKERS;

    fprintf(stdout, "number of workers: %i\n", numWorkers);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
#endif

    /* Prepare word-file */
    int file_descriptor = open(filename_in, O_RDONLY);
    if (file_descriptor < 0)
    { fprintf(stderr, "Cannot open file: %s.\n", filename_in); exit(1); }

    /* Status is used to determine file size */
    if (fstat(file_descriptor, &buf) < 0)
    { fprintf(stderr, "Cannot get info about file.\n"); exit(1); }

    /* Maps the file into memory */
    file_len =       (unsigned)buf.st_size;
    file_beginning = (unsigned char*)mmap(NULL, file_len, PROT_READ, MAP_FILE|MAP_PRIVATE, file_descriptor, 0);
    file_end =       (unsigned char*)(file_beginning + file_len);

    /* Open output file for writing */
    output = fopen(filename_out, "w");
    if (output == NULL)
    { fprintf(stderr, "Cannot open file: %s.\n", filename_out); exit(1); }

    file_offset = file_beginning;

    gettimeofday(&time1, NULL);

#if USE_THREADS
    for (i = 0; i < numWorkers; i++) {
        pthread_create(&workerid[i], &attr, &find_reversed_words, (void*)i);
    }
#else
    (void *)find_reversed_words(NULL);
#endif

#if USE_THREADS
    void *status;
    for (i = 0; i < numWorkers; i++) {
        pthread_join(workerid[i], &status);
    }
#endif

    gettimeofday(&time2, NULL);
    fprintf(stdout, "%f\n", (double)((time2.tv_sec*1000.0+time2.tv_usec/1000.0)-
                                     (time1.tv_sec*1000.0+time1.tv_usec/1000.0)));

    fclose(output);
    munmap(file_beginning, file_len);
    return 0;
}

/*
 * Function: find_reversed_words
 * -------------------
 *
 *   Reads the next word that should be reversed using the
 *   bag of tasks-technique (with an offset). The reversed
 *   word is then used to search through the file for an
 *   occurence.
 *
 *   If the reversed word is a word in the list both the word and
 *   the reversed word are being outputted in an output-file.
 *
 *   arg: Pointer to arg for each thread
 *
 *   returns: Pointer to return data
 */
void *find_reversed_words(void* arg)
{
    char word[WORDSIZE];
    char reversed[WORDSIZE];
    int word_len;

    while (read_word(word) == TRUE) { /* Read a new word from memory (bag of tasks) */
        word_len = strlen(word);
        reverse_word(word, reversed, word_len);

        if (word_exists(reversed, word_len) == TRUE) {
            if (print_to_file(word, reversed) == FALSE) /* 0 bytes written to file */
            { fprintf(stderr, "Error while appending to output-file.\n"); }
        }
    }

#if USE_THREADS
    pthread_exit(NULL);
#endif

    return NULL;
}

/*
 * Function: read_word
 * -------------------
 *
 *   Attemps to read the next word from the file
 *   while protecting the offset with a mutex-lock.
 *
 *   word: reads next word in bag of tasks into 'word'
 *
 *   returns: 1 (TRUE) if word could be read
 *            0 (FALSE) if word could NOT be read
 */
int read_word(char *word)
{
    /* Use variable for return value so that the
     * thread can return lock */
    int return_value = TRUE;
#if USE_THREADS
    pthread_mutex_lock(&lock);
#endif

    if (next_word(word, WORDSIZE-1) == NULL)
        return_value = FALSE;

#if USE_THREADS
    pthread_mutex_unlock(&lock);
#endif

    return return_value;
}

/*
 * Function: next_word
 * -------------------
 *
 *   Takes out the next word from the file starting from
 *   the file_offset. This function is not thread-safe.
 *
 *   s:   Pointer to where the word from the file should
 *        be written to
 *   len: Maximum size of word for pointer s
 *
 *   returns: Pointer to s if successful, NULL if failure
 */
void *next_word(char *s, int len)
{
    int i = 0;
    while (file_offset <= file_end) {

        /* The buffer s is filled or end of word */
        if (i >= len || (*file_offset) == '\n') {

            /* skip newline character */
            if ((*file_offset) == '\n') {
                file_offset++;
                s[i] = '\0';
            } else {
                s[i-1] = '\0';
            }
            return s;
        }
        s[i] = (*file_offset);
        i++;
        file_offset++;
    }

    return NULL;
}

/*
 * Function: reverse_word
 * -------------------
 *
 *   Reverses the word 'from' and puts it in 'to'
 *
 *   from:   Word that should be reversed
 *   to:     Where the reversed word will be written
 *   len:    Length of word
 *
 *   returns: (void)
 */
void reverse_word(char *from, char *to, int len)
{
    int i, j;

    for (j = 0, i = len-1; i >= 0; i--, j++) {
        to[j] = from[i];
    }
    to[len] = '\0';
}

/*
 * Function: word_exists
 * -------------------
 *
 *   Searches through the words-file for the word
 *
 *   word: Word to be searched for
 *   len:  Length of word
 *
 *   returns: 1 (TRUE) if word exists in word list
 *            0 (FALSE) if there is no word
 */
int word_exists(char *word, int len)
{
    unsigned char *pos;
    int i;

    for (pos = file_beginning; pos < file_end; pos++) {

        /* If the word can fit in the rest of the file */
        if ((file_end - pos) >= len && (file_beginning-pos == 0 || pos[-1] == '\n') && pos[len] == '\n') {
            /* Counts the number of same characters of the word */
            for (i = 0; i < len; i++) {
                if (word[i] != pos[i])
                    break;
            }
            /* Reversed word found */
            if (i == len) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

/*
 * Function: print_to_file
 * -------------------
 *
 * Prints the word in the output-file while avoiding race
 * conditions.
 *
 *   word: Word to be written to file
 *
 *   returns: 1 (TRUE) if writing to file was successful
 *            0 (FALSE) if writing to file failed
 */
int print_to_file(char *word, char *reversed)
{
    /* Use variable for return value so that the
     * thread can return lock */
    int return_value = TRUE;
#if USE_THREADS
    pthread_mutex_lock(&lock);
#endif

    if (fprintf(output, "%s: %s\n", word, reversed) == -1)
        return_value = FALSE;

#if USE_THREADS
    pthread_mutex_unlock(&lock);
#endif
    return return_value;
}
