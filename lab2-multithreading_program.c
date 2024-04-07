#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <termios.h>
#include <semaphore.h>

#define SIZE 50

int update_filename1 = 1, update_filename2 = 1, pause1 = 0, pause2 = 0;
pthread_t thread1, thread2;
FILE *file1, *file2;
char filename1[SIZE], filename2[SIZE];
struct termios savetty;
struct termios tty;
sem_t sem1, sem2;

void* read_file1(void* arg)
{
    while(1)
    {
        if (pause1 == 1)
        {
            sem_wait(&sem1);
            pause1 = 0;
        }
        if (update_filename1)
        {
            file1 = fopen(filename1, "r");
            if (file1 == NULL)
            {
                fprintf(stderr, "Can't open file1!\n");
                exit(EXIT_FAILURE);
            }
            update_filename1 = 0;
        }
        char c;
        int bytes_read = fread(&c, sizeof(char), 1, file1);
        if (bytes_read > 0)
        {
            fprintf(stdout, "%c\n", toupper(c));
        }
        else
            return 0;
        sleep(1);
    }
}

void* read_file2(void* arg)
{
    while(1)
    {
        if (pause2 == 1)
        {
            sem_wait(&sem2);
            pause2 = 0;
        }
        if (update_filename2)
        {
            file2 = fopen(filename2, "r");
            if (file2 == NULL)
            {
                fprintf(stderr, "Can't open file2!\n");
                exit(EXIT_FAILURE);
            }
            update_filename2 = 0;
        }
        char c;
        int bytes_read = fread(&c, sizeof(char), 1, file2);
        if (bytes_read > 0)
        {
            fprintf(stdout, "%c\n", tolower(c));
        }
        else
            return 0;
        sleep(1);
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Too few arguments!\n");
        exit(EXIT_FAILURE);
    }

    strcpy(filename1, argv[1]);
    strcpy(filename2, argv[2]);   

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    sem_init(&sem1, 0, 0);
    sem_init(&sem2, 0, 0);
    if (pthread_create(&thread1, &attr, read_file1, NULL))
        perror("pthread_create");
    if (pthread_create(&thread2, &attr, read_file2, NULL))
        perror("pthread_create");
    char tmp_filename[SIZE];
    if ( !isatty(0) ) { 
		fprintf (stderr, "stdin not terminal\n");
		exit (1); 
	};
	tcgetattr (0, &tty);
	savetty = tty; 
	tty.c_lflag &= ~(ICANON);
	tty.c_cc[VMIN] = 1;
	tcsetattr (0, TCSAFLUSH, &tty);
    char sym;
    while(1)
    {
        int bytes_read = read(STDIN_FILENO, &sym, 1);
        if (bytes_read > 0)
        {   
            pause1 = pause2 = 1;
            int i = 0;
            while(sym != '\n')
            {
                tmp_filename[i++] = sym;
                read(STDIN_FILENO, &sym, 1);
                if (i > SIZE - 1) break;
            }
            tmp_filename[i++] = '\0';
            FILE *tmp_file = fopen(tmp_filename, "r");
            if (tmp_file == NULL)
            {
                fprintf(stdout, "Incorrect new file!\n");
            }
            else
            {
                if (tmp_file != NULL) fclose(tmp_file);
                if (file1 != NULL) fclose(file1);
                if (file2 != NULL) fclose(file2);
                strcpy(filename2, filename1);
                strcpy(filename1, tmp_filename);
                update_filename1 = update_filename2 = 1;
            }
            sem_post(&sem1);
            sem_post(&sem2);
            pause1 = pause2 = 0;
        }
        
    }
}
