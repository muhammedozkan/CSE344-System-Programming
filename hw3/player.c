/*---------------------------------------------//
//Gebze Technical University                   //
//CSE344 Systems Programming Course            //
//Muhammed OZKAN 151044084                     //
//---------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

#define SHMPSIZE 4096 //SHAREDMEMORYPROCESSSIZE

typedef struct process
{
	pid_t pid;
	int isPatato;
	int switchNumber;
	char fifoPath[PATH_MAX];
	char fifoName[NAME_MAX];
} process;

typedef struct sharedmemory
{
	int needCoolPatato;
	process processes[SHMPSIZE];
	int processesCount;
	int barrierCount; //fifo count

} sharedmemory;

int _bflag = 0, _sflag = 0, _fflag = 0, _mflag = 0;
int barg = 0;
char *sarg = NULL, *farg = NULL, *marg = NULL;

sharedmemory *share = NULL;
sem_t *share_semaphore = NULL;
sem_t *barrier_semaphore = NULL;
sem_t *readFifo_semaphore = NULL;

sem_t *writeFifo_semaphore[SHMPSIZE];
FILE *writeFifoptr[SHMPSIZE];
int writeFifoFd[SHMPSIZE];

int shmindex = -1;
FILE *fifoptr;
pid_t patato;
int lastrnd = -1;
int flag = 1;
char ownFifoPath[PATH_MAX];
char ownFifoName[NAME_MAX];
int totalProcess = -1;

int randomFifo(int exclude, int max)
{
	int rnd = -1;
	srand(time(NULL) * getpid());
	while (((rnd = (rand() % max)) == exclude) || ((rnd == lastrnd) && (share->barrierCount > 2))) //protect 2 fifo deadlock
	{																							   //repeat if unwanted number is generated
	}
	lastrnd = rnd;
	return rnd;
}

void parseFifoName(char *path, char *getName)
{
	const char s[2] = "/";
	char *token;
	char tmp[PATH_MAX];
	strcpy(tmp, path);
	token = strtok(tmp, s);

	while (token != NULL)
	{
		strcpy(getName, token);
		token = strtok(NULL, s);
	}
}

char *readLineRow(int row)
{
	FILE *fptr = NULL;

	if ((fptr = fopen(farg, "r")) == NULL)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", farg, strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, fptr)) != -1)
	{
		if (count == row)
		{

			if (line[strlen(line) - 2] == '\r')
				line[strlen(line) - 2] = '\0';
			else if (line[strlen(line) - 1] == '\n')
				line[strlen(line) - 1] = '\0';
			fclose(fptr);
			return line;
		}

		count++;
	}

	fclose(fptr);
	return NULL;
}

void attachSHMemory()
{

	int fd;
	fd = shm_open(sarg, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		fprintf(stderr, "(shared memory open)Error! %s dont open because %s %d\n", sarg, strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	if (ftruncate(fd, sizeof(sharedmemory)) == -1)
	{
		fprintf(stderr, "(ftruncate)Error! shared memory dont allocate because %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	share = mmap(NULL, sizeof(*share), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (share == MAP_FAILED)
	{
		fprintf(stderr, "(mmap)Error! dont mapping because %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void attachSHMSemaphore()
{

	if ((share_semaphore = sem_open(sarg, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore SHM)Error! dont create semaphore because %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void attachBarrierSemaphore()
{

	if ((barrier_semaphore = sem_open(marg, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore Barrier)Error! dont create semaphore because %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void attachFifoSemaphore()
{

	if ((readFifo_semaphore = sem_open(share->processes[shmindex].fifoName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore Fifo %s)Error! dont create semaphore because %s\n", share->processes[shmindex].fifoName, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void processRegister()
{
	sem_wait(share_semaphore);
	shmindex = share->processesCount;
	if (share->barrierCount - 1 < shmindex)
	{
		fprintf(stderr, "Error! The process was not created because there were not enough Fifo.\n");
		sem_post(share_semaphore);
		exit(EXIT_FAILURE);
	}

	share->processesCount++;

	if (barg > 0)
	{
		share->needCoolPatato++;
	}

	share->processes[shmindex].pid = getpid();
	share->processes[shmindex].isPatato = barg;
	share->processes[shmindex].switchNumber = 0;
	char *tmp = readLineRow(shmindex);

	strcpy(share->processes[shmindex].fifoPath, tmp);
	free(tmp);
	strcpy(ownFifoPath, share->processes[shmindex].fifoPath);
	parseFifoName(share->processes[shmindex].fifoPath, share->processes[shmindex].fifoName);
	strcpy(ownFifoName, share->processes[shmindex].fifoName);
	sem_post(share_semaphore);
}

void initReadFifo()
{

	if (mkfifo(share->processes[shmindex].fifoPath, S_IRWXU) != 0)
	{
		fprintf(stderr, "Error! %s dont creating because %s\n", share->processes[shmindex].fifoPath, strerror(errno));
		exit(EXIT_FAILURE);
	}
	else
	{
		int fifo_fd = open(share->processes[shmindex].fifoPath, O_RDONLY | O_NONBLOCK);

		if ((fifoptr = fdopen(fifo_fd, "r")) == NULL)
		{
			fprintf(stderr, "Error! %s dont opening(read) because %s\n", share->processes[shmindex].fifoPath, strerror(errno));
			exit(EXIT_FAILURE);
		}

		setvbuf(fifoptr, NULL, _IONBF, 0);
	}
}

int getFifoNum()
{
	FILE *fptr = NULL;

	if ((fptr = fopen(farg, "r")) == NULL)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", farg, strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, fptr)) != -1)
	{
		count++;
	}
	free(line);
	fclose(fptr);
	return count;
}

void writeFifo(int patato, char rndFifo[PATH_MAX])
{
	char writeFifoName[NAME_MAX];
	int sendSmpID = -1;

	parseFifoName(rndFifo, writeFifoName);

	for (int i = 0; i < share->processesCount; i++)
	{
		if (strcmp(share->processes[i].fifoName, writeFifoName) == 0)
		{
			sendSmpID = i;
			break;
		}
	}
	if (patato)
	{

		int switchNumber = 0;

		for (int i = 0; i < share->processesCount; i++)
		{
			if (share->processes[i].pid == patato)
			{
				share->processes[i].switchNumber++;
				switchNumber = share->processes[i].switchNumber;
				break;
			}
		}

		printf("pid=%d sending potato number %d to %s this is switch number %d\n", getpid(), patato, writeFifoName, switchNumber);
	}

	fprintf(writeFifoptr[sendSmpID], "%d\n", patato);
	sem_post(writeFifo_semaphore[sendSmpID]);
}

void readFifo()
{
	while (1) //wait for semaphore not busy waiting!!
	{
		sem_wait(readFifo_semaphore);
		sem_wait(share_semaphore);

		char *line = NULL;
		size_t len = 0;
		getline(&line, &len, fifoptr);
		patato = atoi(line);
		free(line);

		if (patato)
		{
			printf("pid=%d receiving potato number %d from %s\n", getpid(), patato, share->processes[shmindex].fifoName);

			for (int i = 0; i < share->processesCount; i++)
			{
				if (share->processes[i].pid == patato)
				{
					if (share->processes[i].switchNumber == share->processes[i].isPatato)
					{
						printf("pid=%d potato number %d has cooled down.\n", getpid(), patato);
						share->needCoolPatato--;

						if (share->needCoolPatato == 0)
						{
							flag = 0;
							for (int i = 0; i < share->processesCount; i++)
							{
								if (i != shmindex)
								{
									char rndFifo[PATH_MAX];
									char *tmp = readLineRow(i);
									strcpy(rndFifo, tmp);
									free(tmp);
									writeFifo(0, rndFifo);
								}
							}
						}
					}
					else
					{
						char rndFifo[PATH_MAX];
						char *tmp = readLineRow(randomFifo(shmindex, share->processesCount));
						strcpy(rndFifo, tmp);
						free(tmp);
						writeFifo(patato, rndFifo);
					}

					break;
				}
			}
		}
		else
		{
			flag = 0;
		}

		sem_post(share_semaphore);
		if (flag)
		{
		}
		else
		{
			break;
		}
	}
}

void freeresource()
{
	if (remove(ownFifoPath) != 0)
		fprintf(stderr, "Error! %s Unable to delete the file %s\n", ownFifoPath, strerror(errno));

	if (sem_close(readFifo_semaphore) != 0)
		fprintf(stderr, "Error! close readFifo semaphore : %s\n", strerror(errno));

	if (sem_unlink(ownFifoName) != 0)
		fprintf(stderr, "Error! semaphore %s unlink: %s\n", ownFifoName, strerror(errno));

	if (sem_close(share_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(barrier_semaphore) != 0)
		fprintf(stderr, "Error! close barrier semaphore : %s\n", strerror(errno));

	for (int i = 0; i < totalProcess; i++)
	{
		sem_close(writeFifo_semaphore[i]);
		fclose(writeFifoptr[i]);
	}

	sem_unlink(sarg);
	sem_unlink(marg);
	shm_unlink(sarg);
	fclose(fifoptr);
}

//for CTRL-C signal catch
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	flag = 0;
	for (int i = 0; i < totalProcess; i++)
	{
		char rndFifo[PATH_MAX];
		char *tmp = readLineRow(i);
		strcpy(rndFifo, tmp);
		free(tmp);
		writeFifo(0, rndFifo);
	}

	char error[56];
	strcpy(error, "\nProgram terminated because CTRL-C keys were pressed.\n");
	write(fileno(stdout), error, strlen(error));
}

int main(int argc, char *argv[])
{
	signal(SIGINT, INThandler);
	int opt;

	while ((opt = getopt(argc, argv, "b:s:f:m:")) != -1)
	{
		switch (opt)
		{

		case 'b':
			_bflag = 1;

			if ((optarg[0] == '0' || optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '3' || optarg[0] == '4' || optarg[0] == '5' || optarg[0] == '6' || optarg[0] == '7' || optarg[0] == '8' || optarg[0] == '9'))

				if (atoi(optarg) >= 0)
				{
					barg = atoi(optarg);
				}
				else
				{
					fprintf(stderr, "The -b parameter must be 0 or greater than 0. Program terminate\n");
					exit(EXIT_FAILURE);
				}
			else
			{
				fprintf(stderr, "The -b parameter must be an unsigned integer. Program terminate\n");
				exit(EXIT_FAILURE);
			}

			break;

		case 's':
			_sflag = 1;
			sarg = optarg;

			break;

		case 'f':
			_fflag = 1;
			farg = optarg;

			break;

		case 'm':
			_mflag = 1;
			marg = optarg;

			break;

		case '?':
			if (optopt == 'b' || optopt == 's' || optopt == 'f' || optopt == 'm')
				fprintf(stderr, "Option '-%c' requires an parameter. Program terminate\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown parameter '-%c'. Program terminate\n", optopt);
			else
				fprintf(stderr, "Unknown parameter character '\\x%x'. Program terminate\n", optopt);
			exit(EXIT_FAILURE);
			break;

		default:
			fprintf(stderr, "Unknown Error. Program terminate\n");
			exit(EXIT_FAILURE);
			break;
		}
	}

	if (!(_bflag == 1 && _sflag == 1 && _fflag == 1 && _mflag == 1))
	{
		fprintf(stderr, "You must use all parameter. -b -s -f -m. Program terminate\n");
		exit(EXIT_FAILURE);
	}

	for (; optind < argc; optind++)
	{
		fprintf(stderr, "Error extra parameter: '%s'. Program terminate\n", argv[optind]);
		exit(EXIT_FAILURE);
	}

	attachSHMSemaphore();
	attachBarrierSemaphore();
	attachSHMemory();

	sem_wait(share_semaphore);
	if (share->barrierCount == 0)
	{
		share->barrierCount = getFifoNum();
		if (share->barrierCount <= 1)
		{
			fprintf(stderr, "The fifo file must contain more than 1 fifo paths. Program terminate\n");
			sem_post(share_semaphore);
			sem_unlink(sarg);
			sem_unlink(marg);
			shm_unlink(sarg);
			exit(EXIT_FAILURE);
		}
	}
	sem_post(share_semaphore);
	processRegister();
	attachFifoSemaphore();
	initReadFifo();

	printf("PID=%d Waiting Other Processes\n", getpid());

	if (share->processesCount == share->barrierCount)
		sem_post(barrier_semaphore);

	sem_wait(barrier_semaphore);
	sem_post(barrier_semaphore);
	//barrier

	sem_wait(share_semaphore);
	totalProcess = share->processesCount;
	for (int i = 0; i < totalProcess; i++)
	{
		if ((writeFifo_semaphore[i] = sem_open(share->processes[i].fifoName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
		{
			fprintf(stderr, "(semaphore Fifo %s)Error! dont create semaphore because %s\n", share->processes[i].fifoName, strerror(errno));
			sem_post(share_semaphore);
			exit(EXIT_FAILURE);
		}

		writeFifoFd[i] = open(share->processes[i].fifoPath, O_WRONLY | O_NONBLOCK);

		if ((writeFifoptr[i] = fdopen(writeFifoFd[i], "w")) == NULL)
		{
			fprintf(stderr, "Error! %s dont opening(write) because %s\n", share->processes[i].fifoPath, strerror(errno));
			sem_post(share_semaphore);
			exit(EXIT_FAILURE);
		}
		setvbuf(writeFifoptr[i], NULL, _IONBF, 0);
	}

	sem_post(share_semaphore);

	if (barg > 0)
	{
		sem_wait(share_semaphore);
		char rndFifo[PATH_MAX];
		char *tmp = readLineRow(randomFifo(shmindex, share->processesCount));
		strcpy(rndFifo, tmp);
		free(tmp);
		writeFifo(getpid(), rndFifo);
		sem_post(share_semaphore);
	}
	readFifo();

	freeresource();

	return 0;
}