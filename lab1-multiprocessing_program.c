#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#define SIZE 50

char filename1[SIZE]; //название первого файла
char filename2[SIZE]; //название второго файла
int pipe1[2]; // для обмена данными между родительским процессом и 1 дочерним процессом
int pipe2[2]; // для обмена данными между 2 дочерними процессами,
int update_filename1 = 0; //флаги, показывающие что нужно обновить название файла в 1 (2) процессе
int update_filename2 = 0;
struct termios savetty;
struct termios tty;

//обработчик сигнала SIGUSR1 для первого дочернего процесса
// второй порожденный процесс переходит к обработке файла, ранее обрабатываемого первым процессом
void signal_handler1()
{
    write(pipe2[1], filename1, sizeof(char) * SIZE);
    read(pipe1[0], filename1, sizeof(char) * SIZE);
    update_filename1 = 1;
}

void signal_handler2() //обработчик сигнала SIGUSR1 для второго дочернего процесса
{
    read(pipe2[0], filename2, sizeof(char) * SIZE);
    update_filename2 = 1;
}

void disable_unconical_input()
{
	tcsetattr(0, TCSAFLUSH, &savetty);
	kill(getpid(), SIGTERM);
}

int main(int argc, char **argv)
{
    if (argc != 3) //проверка количества аргументов
    {
        fprintf(stderr, "Too few arguments!\n");
        exit(EXIT_FAILURE);
    }
    strcpy(filename1, argv[1]);
    strcpy(filename2, argv[2]);
    if (pipe(pipe1)) //создание 1 канала обмена данными между потоками
    {
        fprintf(stderr, "Failed to create pipe!\n");
        exit(EXIT_FAILURE);
    }
    if (pipe(pipe2)) //создание 2 канала обмена данными между потоками
    {
        fprintf(stderr, "Failed to create pipe!\n");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, disable_unconical_input);
    signal(SIGKILL, disable_unconical_input);
    int pid1 = fork(); //создание 1 процесса
    if (pid1 < 0)
    {
        fprintf(stderr, "Fork failed!\n");
        exit(EXIT_FAILURE);
    }
    if (pid1 == 0) // в этом блоке весь код для 1 процесса
    {
        FILE *file1 = fopen(filename1, "r"); //открытие файла в режиме чтения
        if (file1 == NULL)
        {
            fprintf(stderr, "Can't open file1!\n");
            exit(EXIT_FAILURE);
        }
        signal(SIGUSR1, signal_handler1);
        char c;
        do
        {
            if (update_filename1) // срабатывает один раз в случае если пользователь ввел в stdin корректное название файла (обновление файла)
            {
                fclose(file1);
                file1 = fopen(filename1, "r");
                if (file1 == NULL)
                {
                    fprintf(stderr, "Can't open file1!\n");
                    exit(EXIT_FAILURE);
                }
                update_filename1 = 0;
            }
            int byte = fread(&c, sizeof(char), 1, file1); //чтение 1 байта из файла
            if (byte > 0)
            {
                fprintf(stdout, "%c\n", toupper(c)); //вывод по одному байту из файла с интервалом 1 секунда
            }
            sleep(1);
        } while (1);
    }
    int pid2 = fork(); //всё аналогично 1 процессу
    if (pid2 < 0)
    {
        fprintf(stderr, "Fork failed!\n");
        exit(EXIT_FAILURE);
    }
    if (pid2 == 0)
    {
        FILE *file2 = fopen(argv[2], "r");
        if (file2 == NULL)
        {
            fprintf(stderr, "Can't open file2!\n");
            exit(EXIT_FAILURE);
        }
        signal(SIGUSR1, signal_handler2);
        char c;
        do
        {
            if (update_filename2)
            {
                fclose(file2);
                file2 = fopen(filename2, "r");
                if (file2 == NULL)
                {
                    fprintf(stderr, "Can't open file2!\n");
                    exit(EXIT_FAILURE);
                }
                update_filename2 = 0;
            }
            int r_bytes = fread(&c, sizeof(char), 1, file2);
            if (r_bytes > 0)
            {
                fprintf(stdout, "%c\n", tolower(c));
            }
            sleep(1);
        } while (1);
        pause();
    }
    if ( !isatty(0) ) { /*Проверка: стандартный ввод - терминал?*/
		fprintf (stderr, "stdin not terminal\n");
		exit (1); /* Ст. ввод был перенаправлен на файл, канал и т.п. */
	};
	tcgetattr (0, &tty);
	savetty = tty; /* Сохранить упр. информацию канонического режима */
	tty.c_lflag &= ~(ICANON);
	tty.c_cc[VMIN] = 1;
	tcsetattr (0, TCSAFLUSH, &tty);
    char filename[SIZE];
    char sym;
    while(1)
    {
        //int bytes_read = scanf("%s", filename); //если пользователь ввел что-то и нажал enter, проверим есть ли такой файл
        int bytes_read = read(STDIN_FILENO, &sym, 1);
        if (bytes_read > 0)
        {
            kill(pid1, SIGSTOP);
            kill(pid2, SIGSTOP);
            int i = 0;
            while(sym != '\n')
            {
                filename[i++] = sym;
                read(STDIN_FILENO, &sym, 1);
                if (i > SIZE - 1) break;
            }
            filename[i++] = '\0';
            FILE *tmp_file = fopen(filename, "r");
            if (tmp_file != NULL)
            {
                fclose(tmp_file);
                write(pipe1[1], filename, SIZE);
                kill(pid1, SIGUSR1); //отправка сигнала, для активации функции signal_handler_1();
                kill(pid2, SIGUSR1);
            }
            else
            {
                fprintf(stdout, "Can't open file! Enter filename again.\n");
            }
            kill(pid1, SIGCONT);
            kill(pid2, SIGCONT);
        }
    }
    kill(pid1, SIGTERM);
    kill(pid2, SIGTERM);
    exit(EXIT_SUCCESS);
}
