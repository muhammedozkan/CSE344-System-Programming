/*---------------------------------------------//
//Gebze Technical University                   //
//CSE344 Systems Programming Course            //
//Muhammed OZKAN 151044084                     //
//---------------------------------------------*/

#include <stdio.h>
#include <stdlib.h> // For exit() function
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
FILE * fptr;
int childrenPID[8];

int usrsig1 = 0;

//for CTRL-C signal catch
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	
	char error[56];
	strcpy(error,"\nProgram terminated because CTRL-C keys were pressed.\n");
	write(fileno(stdout),error,strlen(error));
	raise(SIGINT);
}

void sigusr2() {
  //emptyhandler
}

void sigusr1() {
  usrsig1++;
  
  if (usrsig1 == 8) {
    raise(SIGUSR2);
  }
  if (usrsig1 == 16) {
    raise(SIGUSR2);
  }
}



double lagrange(double * x, double * y, int n, double xp) {
  double yp = 0, p;

  for (int i = 0; i < n; i++) {
    p = 1;
    for (int j = 0; j < n; j++) {
      if (i != j) {
        p = p * (xp - x[j]) / (x[i] - x[j]);
      }
    }
    yp = yp + p * y[i];
  }

  return yp;
}
//https://www.bragitoff.com/2018/06/polynomial-fitting-c-program///
/*******
 Function that performs Gauss-Elimination and returns the Upper triangular matrix and solution of equations:
There are two options to do this in C.
1. Pass the augmented matrix (a) as the parameter, and calculate and store the upperTriangular(Gauss-Eliminated Matrix) in it.
2. Use malloc and make the function of pointer type and return the pointer.
This program uses the first option.
********/
void gaussEliminationLS(int m, int n, double a[m][n], double x[n - 1]) {
  int i, j, k;
  for (i = 0; i < m - 1; i++) {
    //Partial Pivoting
    for (k = i + 1; k < m; k++) {
      //If diagonal element(absolute vallue) is smaller than any of the terms below it
      if (fabs(a[i][i]) < fabs(a[k][i])) {
        //Swap the rows
        for (j = 0; j < n; j++) {
          double temp;
          temp = a[i][j];
          a[i][j] = a[k][j];
          a[k][j] = temp;
        }
      }
    }
    //Begin Gauss Elimination
    for (k = i + 1; k < m; k++) {
      double term = a[k][i] / a[i][i];
      for (j = 0; j < n; j++) {
        a[k][j] = a[k][j] - term * a[i][j];
      }
    }

  }
  //Begin Back-substitution
  for (i = m - 1; i >= 0; i--) {
    x[i] = a[i][n - 1];
    for (j = i + 1; j < n - 1; j++) {
      x[i] = x[i] - a[i][j] * x[j];
    }
    x[i] = x[i] / a[i][i];
  }

}

void writeLineEndofRow(FILE * fptr, int rows, double value) {


	char buf[64];
	sprintf(buf, "%lf\n", value);
	
	fseek(fptr, 0L, SEEK_END);
	char * text = (char * ) malloc(1 + ftell(fptr)+strlen(buf));
	
  rewind(fptr);
  
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  int count = 0;
  strcpy(text, "");
  
  while ((read = getline( & line, & len, fptr)) != -1) {

	if (count == rows) {
      strcat(text, line);
      text[strlen(text) - 1] = ',';
      strcat(text, buf);

    } else {
      strcat(text, line);
    }

    count++;
  }

  rewind(fptr);

  int results = fputs(text, fptr);
  if (results == EOF) {
    fprintf(stderr, "CHID : %d PID: %d File WRITE error : %s ", getpid(), getppid(), strerror(errno));
	exit(EXIT_FAILURE);
  }

  free(line);
  free(text);
}

char * readLineRow(FILE * fptr, int row) {

  rewind(fptr);

  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  int count = 0;

  while ((read = getline( & line, & len, fptr)) != -1) {
    if (count == row) {

      return line;
    }

    count++;
  }

  return line;
}

void parseDouble(char * line, double * arr, int size) {
  const char s[2] = ",";
  char * token;
  int i = 0;
  token = strtok(line, s);

  while (token != NULL) {
    if (i < size) {
      arr[i] = atof(token);
      i++;
    }

    token = strtok(NULL, s);
  }

}

void fileLock(FILE * fptr) {
  struct flock lock;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_pid = getpid();

  if (fcntl(fileno(fptr), F_SETLKW, & lock) == -1) {
    fprintf(stderr, "CHID : %d PID: %d F_SETLKW LOCK error : %s ", getpid(), getppid(), strerror(errno));
	exit(EXIT_FAILURE);
  }

}

void fileUnlock(FILE * fptr) {
  struct flock lock;
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_pid = getpid();

  if (fcntl(fileno(fptr), F_SETLKW, & lock) == -1) {
    fprintf(stderr, "CHID : %d PID: %d F_SETLKW UNLOCK error : %s ", getpid(), getppid(), strerror(errno));
	exit(EXIT_FAILURE);
  }

}

void children(int childNo, FILE * fptr) {

  signal(SIGUSR2, sigusr2);
  signal(SIGINT, sigusr2);
  sigset_t sigset;

  sigemptyset(&sigset);


  char * a;
  double d[16];
  double x[8], y[8];
  double temp;


  fileLock(fptr);

  a = readLineRow(fptr, childNo);

  fileUnlock(fptr);

  parseDouble(a, d, 16);
  free(a);
  x[0] = d[0];
  y[0] = d[1];
  x[1] = d[2];
  y[1] = d[3];
  x[2] = d[4];
  y[2] = d[5];
  x[3] = d[6];
  y[3] = d[7];
  x[4] = d[8];
  y[4] = d[9];
  x[5] = d[10];
  y[5] = d[11];
  x[6] = d[12];
  y[6] = d[13];
  x[7] = d[14];
  y[7] = d[15];

  temp = lagrange(x, y, 6, x[7]);

  fileLock(fptr);

  writeLineEndofRow(fptr, childNo, temp);

  fileUnlock(fptr);

  kill(getppid(), SIGUSR1);
  
  
  sigsuspend(&sigset);
  signal(SIGUSR2, sigusr2);
  signal(SIGINT, sigusr2);

  temp = lagrange(x, y, 7, x[7]);

  fileLock(fptr);

  writeLineEndofRow(fptr, childNo, temp);

  fileUnlock(fptr);

  kill(getppid(), SIGUSR1);
  
  
  sigsuspend(&sigset);
signal(SIGUSR2, sigusr2);
  signal(SIGINT, sigusr2);
 

  //https://www.bragitoff.com/2018/06/polynomial-fitting-c-program///

  int N = 7; //datasize
  int n = 6; //degree

  // an array of size 2*n+1 for storing N, Sig xi, Sig xi^2, ...., etc. which are the independent components of the normal matrix
  double X[2 * n + 1];
  for (int i = 0; i <= 2 * n; i++) {
    X[i] = 0;
    for (int j = 0; j < N; j++) {
      X[i] = X[i] + pow(x[j], i);
    }
  }
  //the normal augmented matrix
  double B[n + 1][n + 2];
  // rhs
  double Y[n + 1];
  for (int i = 0; i <= n; i++) {
    Y[i] = 0;
    for (int j = 0; j < N; j++) {
      Y[i] = Y[i] + pow(x[j], i) * y[j];
    }
  }
  for (int i = 0; i <= n; i++) {
    for (int j = 0; j <= n; j++) {
      B[i][j] = X[i + j];
    }
  }
  for (int i = 0; i <= n; i++) {
    B[i][n + 1] = Y[i];
  }
  double A[n + 1];
  gaussEliminationLS(n + 1, n + 2, B, A);
  
  printf("Polynomial %d: ", childNo);
  
  for (int i = 0; i <= n; i++) {
    printf("%0.1lf", A[i]);
    if (i < n)
      printf(",");
  }
  printf("\n");
}

void parent(FILE * fptr) {
  
	
	sigset_t sigset;

    sigfillset(&sigset);
sigdelset(&sigset, SIGUSR1);
    sigdelset(&sigset, SIGUSR2);
	sigdelset(&sigset, SIGINT);
	signal(SIGUSR1, sigusr1);
	signal(SIGUSR2, sigusr2);
	signal(SIGINT, INThandler);
	signal(SIGCHLD, sigusr2);
	
  char * a;
  double d[18];
  double degree5 = 0;
  double degree6 = 0;

	sigsuspend(&sigset);
signal(SIGUSR1, sigusr1);
	signal(SIGUSR2, sigusr2);
	signal(SIGINT, INThandler);


  fileLock(fptr);

  for (int i = 0; i < 8; i++) {
    a = readLineRow(fptr, i);

    parseDouble(a, d, 17);
    free(a);
    degree5 += fabs(d[15] - d[16]);

  }

  fileUnlock(fptr);

  printf("Error of polynomial of degree 5: %0.1lf\n", (degree5 / 8));

  for (int i = 0; i < 8; i++)
    kill(childrenPID[i], SIGUSR2);


  sigsuspend(&sigset);
signal(SIGUSR1, sigusr1);
	signal(SIGUSR2, sigusr2);
	signal(SIGINT, INThandler);


  fileLock(fptr);

  for (int i = 0; i < 8; i++) {
    a = readLineRow(fptr, i);

    parseDouble(a, d, 18);
    free(a);
    degree6 += fabs(d[15] - d[17]);

  }

  fileUnlock(fptr);

  printf("Error of polynomial of degree 6: %0.1lf\n", (degree6 / 8));

  for (int i = 0; i < 8; i++)
    kill(childrenPID[i], SIGUSR2);

}

int main(int argc, char * argv[]) {

  signal(SIGUSR1, sigusr1);

  //setvbuf(stdout, NULL, _IONBF, 0);

  if (argc != 2) {
    fprintf(stderr, "Please enter command line parameters!\n\t\tUsage: ./processM pathToFile\n");
    exit(0);
  }

  if ((fptr = fopen(argv[1], "r+")) == NULL) {
    fprintf(stderr, "Error! %s dont opening because %s\n", argv[1], strerror(errno));
    exit(EXIT_FAILURE);
  }
  setvbuf(fptr, NULL, _IONBF, 0);

  childrenPID[0] = fork();
  if (childrenPID[0] > 0) {
    childrenPID[1] = fork();
    if (childrenPID[1] > 0) {
      childrenPID[2] = fork();
      if (childrenPID[2] > 0) {
        childrenPID[3] = fork();
        if (childrenPID[3] > 0) {
          childrenPID[4] = fork();
          if (childrenPID[4] > 0) {
            childrenPID[5] = fork();
            if (childrenPID[5] > 0) {
              childrenPID[6] = fork();
              if (childrenPID[6] > 0) {
                childrenPID[7] = fork();
                if (childrenPID[7] > 0) {

                  parent(fptr);
 			
                } else if (childrenPID[7] == 0) {
                  children(7, fptr);
                }
              } else if (childrenPID[6] == 0) {
                children(6, fptr);
              }
            } else if (childrenPID[5] == 0) {
              children(5, fptr);
            }
          } else if (childrenPID[4] == 0) {
            children(4, fptr);
          }
        } else if (childrenPID[3] == 0) {
          children(3, fptr);
        }
      } else if (childrenPID[2] == 0) {
        children(2, fptr);
      }
    } else if (childrenPID[1] == 0) {
      children(1, fptr);
    }
  } else if (childrenPID[0] == 0) {
    children(0, fptr);
  }

  fclose(fptr);

  return 0;
}
