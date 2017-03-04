#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <memory.h>
#define _PIPE 1
#define _FIFO 2
#define _SOCKET 3
#define FIFO_FILE "FIFO_FILE_216"
#define FIFO_FILE2 "FIFO_FILE_2_216"

int PIPE_TYPE;
int fds[4];

// 0 - citesc copil, 1 - scriu parinte, 2 - citesc parinte, 3 - scriu copil

void OPEN_PIPE() {
  switch(PIPE_TYPE) {
    case _PIPE: {
      if(pipe(fds) == -1 || pipe(fds + 2) == -1) {
        fprintf(stderr, "Eroare la deschiderea pipe\n");
        exit(-1);
      }
      break;
    }

    case _FIFO: {
      mkfifo(FIFO_FILE, 0666);
      mkfifo(FIFO_FILE2, 0666);

      fds[0] = open(FIFO_FILE, O_RDONLY | O_NONBLOCK);
      fds[1] = open(FIFO_FILE, O_WRONLY);
      fds[2] = open(FIFO_FILE2, O_RDONLY | O_NONBLOCK);
      fds[3] = open(FIFO_FILE2, O_WRONLY);

      int flags;

      flags = fcntl(fds[0], F_GETFL, 0);
      flags ^= O_NONBLOCK;
      fcntl(fds[0], F_SETFL, flags);

      flags = fcntl(fds[2], F_GETFL, 0);
      flags ^= O_NONBLOCK;
      fcntl(fds[2], F_SETFL, flags);
      break;
    }

    case _SOCKET: {
      if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        fprintf(stderr, "Erroare la deschiderea socketpair");
        exit(-1);
      }

      if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds + 2) < 0) {
        fprintf(stderr, "Erroare la deschiderea socketpair");
        exit(-1);
      }
      break;
    }
  }
}

void CLOSE_PIPE() {
  switch(PIPE_TYPE) {
    case _PIPE: {
      close(fds[0]); close(fds[1]);
      close(fds[2]); close(fds[3]);
    }

    case _FIFO: {
      close(fds[0]); close(fds[1]);
      close(fds[2]); close(fds[3]);
      unlink(FIFO_FILE);
      unlink(FIFO_FILE2);
    }

    case _SOCKET: {
      close(fds[0]); close(fds[1]);
      close(fds[2]); close(fds[3]);
    }
  }
}

void MYFIND(char* now, char* Name) {
  struct stat stat_file;
  struct dirent *dir_curr;
  DIR* dir;
  static char FOUND = 0;

  if((dir = opendir(now)) == NULL) return;

  while((dir_curr = readdir(dir)) != NULL && !FOUND) {
    stat(now, &stat_file);

    if(S_ISDIR(stat_file.st_mode)) {
      if(!strcmp(dir_curr->d_name, ".") || !strcmp(dir_curr->d_name, "..")) continue;

      int i, n = 5 + strlen(now) + strlen(dir_curr->d_name);
      char* next_dir = (char*)malloc(n);
      for(i = 0;i < n; ++i) next_dir[i] = 0;
      strcpy(next_dir, now);
      strcat(next_dir, "/");
      strcat(next_dir, dir_curr->d_name);
      MYFIND(next_dir, Name);

      if(FOUND) break;

      char ok;
      i = 0;
      ok = 1;
      n = strlen(dir_curr->d_name);
      if(strlen(Name) != n) ok =0;

      for(i = 0; i < n && ok; ++i)
        if(dir_curr->d_name[i] != Name[i]) ok = 0;

      if(ok) {
        FOUND = 1;

        char buff[256];
        memset(buff, 0, sizeof(buff));

        struct stat stat_file;
        stat(next_dir, &stat_file);

        int pref_len = 0;

        #ifdef HAVE_ST_BIRTHTIME
        #define nastere(x) x.st_birthtime
        #else
        #define nastere(x) x.st_ctime
        #endif

        snprintf(buff, sizeof(buff), "\nSize: %d b\nUltima modificare: %sUltima accesare: %sData crearii: %s", 
                                     (int)stat_file.st_size,
                                     asctime(localtime(&(stat_file.st_mtime))),
                                     asctime(localtime(&(stat_file.st_atime))),
                                     asctime(localtime(&(nastere(stat_file)))));
        pref_len = strlen(buff) + 10;

        write(fds[3], (char*)(&pref_len), sizeof(pref_len));
        write(fds[3], (S_ISDIR(stat_file.st_mode)) ? "d" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IRUSR) ? "r" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IWUSR) ? "w" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IXUSR) ? "x" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IRGRP) ? "r" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IWGRP) ? "w" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IXGRP) ? "x" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IROTH) ? "r" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IWOTH) ? "w" : "-", 1);
        write(fds[3], (stat_file.st_mode & S_IXOTH) ? "x" : "-", 1);
        write(fds[3], buff, strlen(buff));

        break;
      }
    }
  }

  if(now[0] == '.' && strlen(now) == 1 && !FOUND) {
    int pref_len = 28;
    write(fds[3], (char*)&pref_len, sizeof(pref_len));
    write(fds[3], "Nu am gasit fisierul dorit.\n", pref_len);
  }
}

int OK_LOG(char* User, char* Pass) {
  switch(fork()) {
    case -1: {
      fprintf(stderr, "Eroare la fork\n");
      exit(-1);
    }
    case 0: {
      // fiu
      FILE *fr;
      fr = fopen("Users", "r");
      if(!fr) {
        fprintf(stderr, "Eroare la open\n");
        exit(-1);
      }

      char File_user[64], now;
      memset(File_user, 0, sizeof(File_user));

      while(1) {
        now = fgetc(fr);
        if(now == EOF) break;

        if(now != ' ') {
          strncat(File_user, &now, 1);
          continue;
        }

        char buff_pass[256];
        memset(buff_pass, 0, sizeof(buff_pass));

        while(1) {
          now = fgetc(fr);
          if(now == EOF || now == '\n') break;
          strncat(buff_pass, &now, 1);
        }

        if(!strcmp(File_user, User)) {
          int pref_len = strlen(buff_pass);
          write(fds[3], (char*)&pref_len, sizeof(pref_len));
          write(fds[3], buff_pass, strlen(buff_pass));
          fclose(fr);
          exit(0);
        }

        memset(File_user, 0, sizeof(File_user));
      }

      fclose(fr);
      exit(1);
    }
    default: {
      int pref_len = strlen(User);
      write(fds[1], (char*)&pref_len, sizeof(pref_len));
      write(fds[1], User, strlen(User));

      int ans = 0;
      wait(&ans);

      if(ans) return ans;

      char buff[256];
      memset(buff, 0, sizeof(buff));

      read(fds[2], (char*)&pref_len, sizeof(pref_len));
      read(fds[2], buff, pref_len);

      return strcmp(buff, Pass);
    }
  }

  return -1;
}

void MyFind() {
  switch(fork()) {
    case -1: {
      fprintf(stderr, "Eroare la fork\n");
      exit(-1);
    }

    case 0: {
      char buff[256];
      memset(buff, 0, sizeof(buff));

      int n = 0;
      read(fds[0], (char*)(&n), sizeof(n));
      read(fds[0], buff, n);

      MYFIND(".", buff);

      exit(0);
    }

    default: {
      char file[256];
      memset(file, 0, sizeof(file));

      printf("Numele fisierului: ");
      gets(file);
      int len = strlen(file);
      write(fds[1], (char*)(&len), sizeof(len));
      write(fds[1], file, len);

      wait(NULL);

      memset(file, 0, sizeof(file));

      int n = 0;
      read(fds[2], (char*)(&n), sizeof(n));
      read(fds[2], file, n);

      printf("\n%s", file);

      break;
    }
  }
}

void MyStat() {
  switch(fork()) {
    case -1: {
      fprintf(stderr, "Eroare la fork\n");
      exit(-1); 
    }

    case 0: {
      char buff[256];
      int pref_len = 0;
      memset(buff, 0, sizeof(buff));

      read(fds[0], (char*)&pref_len, sizeof(pref_len));
      read(fds[0], buff, pref_len);
      struct stat stat_file;
      stat(buff, &stat_file);

      if(!S_ISREG(stat_file.st_mode) && !S_ISDIR(stat_file.st_mode)) {
        snprintf(buff, sizeof(buff), "Fisierul nu exista in directorul curent!!\n");
        pref_len = strlen(buff);
        write(fds[3], (char*)&pref_len, sizeof(pref_len));
        write(fds[3], buff, pref_len);
        exit(1);
      }

      snprintf(buff, sizeof(buff), "\nSize: %d b\n", (int)stat_file.st_size);

      pref_len = strlen(buff) + 10;

      write(fds[3], (char*)&pref_len, sizeof(pref_len));
      write(fds[3], (S_ISDIR(stat_file.st_mode)) ? "d" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IRUSR) ? "r" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IWUSR) ? "w" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IXUSR) ? "x" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IRGRP) ? "r" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IWGRP) ? "w" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IXGRP) ? "x" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IROTH) ? "r" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IWOTH) ? "w" : "-", 1);
      write(fds[3], (stat_file.st_mode & S_IXOTH) ? "x" : "-", 1);
      write(fds[3], buff, strlen(buff));

      exit(0);
    }

    default: {
      printf("Introduceti numele fisierului relativ la calea curenta: ");
      char buff[256];
      int pref_len = 0;
      memset(buff, 0, sizeof(buff));

      gets(buff);

      pref_len = strlen(buff);
      write(fds[1], (char*)&pref_len, sizeof(pref_len));
      write(fds[1], buff, pref_len);

      wait(NULL);

      memset(buff, 0, sizeof(buff));

      read(fds[2], (char*)(&pref_len), sizeof(pref_len));
      read(fds[2], buff, sizeof(buff));

      printf("\n%s", buff);
      break;
    }
  }
}

int main(int argc, char* argv[])
{
  puts("1) Pipe\n2) FIFO\n3) Socket");
  puts("Introduceti numarul dorit\nDaca introduceti ceva gresit, o sa consider ca ati ales PIPE");
  char wish[128];
  memset(wish, 0, sizeof(wish));
  gets(wish);

  PIPE_TYPE = wish[0] - '0';

  if(PIPE_TYPE < 1 || PIPE_TYPE > 3) PIPE_TYPE = _PIPE;

  OPEN_PIPE();

  while(1) {
    printf("\nUsername: ");
    char User[128], Pass[128];
    memset(User, 0, sizeof(User));
    memset(Pass, 0, sizeof(Pass));
    gets(User);
    printf("\nPassword: ");
    gets(Pass);

    if(OK_LOG(User, Pass)) printf("Fail la logare, incercati din nou.\n");
    else {
      puts("Te-ai logat cu succes!\n");

      while(1) {
        puts("\nMeniu:\n1) MyFind\n2) MyStat\n3) Quit");
        printf("Alegeti comanda dorita (numarul din stanga comenzii): ");
        CLOSE_PIPE();
        OPEN_PIPE();

        char comm[128];
        memset(comm, 0, sizeof(comm));
        gets(comm);

        if(comm[0] < '1' || comm[0] > '3') {
          puts("Ai gresit comanda, incearca din nou!\n");
          continue;
        }

        if(comm[0] == '3') {
          puts("Good bye");
          CLOSE_PIPE();
          return 0;
        }

        if(comm[0] == '1') {
          MyFind();
          continue;
        }

        if(comm[0] == '2') {
          MyStat();
          continue;
        }
      }
    }
  }

  return 0;
}