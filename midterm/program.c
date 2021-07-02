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
#include <sys/wait.h>
#include <fcntl.h>
#include <semaphore.h>

#define SIZE 4096
#define SHMNAME "ortak151044084"
#define EMPTY "empty151044084"
#define FIRST "first151044084"
#define SECOND "second151044084"

typedef struct citizen
{
	pid_t pid;
	int dozeCount;
	int remaining;
	int pipefd[2];
} citizen;

typedef struct vaccinator
{
	pid_t pid;
	int dozeCount;
} vaccinator;

typedef struct sharedmemory
{
	citizen citizens[SIZE];
	vaccinator vaccinators[SIZE];
	int citizenCount;
	int carryEnd;
	int vaccineEnd;
	int vaccineCount;
	int firstShotBuffer;
	int secondShotBuffer;
	int receiveVaccine;
	int totalVaccinedCitizen;
} sharedmemory;

int _nflag = 0, _vflag = 0, _cflag = 0, _bflag = 0, _tflag = 0, _iflag = 0;
int _narg = 0, _varg = 0, _carg = 0, _barg = 0, _targ = 0;
char *_iarg = NULL;

int fd;

sharedmemory *share = NULL;
sem_t *share_semaphore = NULL;
sem_t *empty_semaphore = NULL;
sem_t *first_semaphore = NULL;
sem_t *second_semaphore = NULL;

//for CTRL-C signal catch
void INThandler(int sig)
{
	char error[56];
	strcpy(error, "\nProgram terminated because CTRL-C keys were pressed.\n");
	write(fileno(stdout), error, strlen(error));

	int status;
	int n = _narg + _varg + _carg;
	while (n > 0)
	{
		wait(&status);
		--n;
	}

	close(fd);

	if (sem_close(share_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(empty_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(first_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(second_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	sem_unlink(SHMNAME);
	sem_unlink(EMPTY);
	sem_unlink(FIRST);
	sem_unlink(SECOND);
	shm_unlink(SHMNAME);

	exit(EXIT_FAILURE);
}

void INTChildhandler(int sig)
{
	if (sem_close(share_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(empty_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(first_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(second_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	exit(EXIT_FAILURE);
}

void closeSemaphore()
{
	if (sem_close(share_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(empty_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(first_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));

	if (sem_close(second_semaphore) != 0)
		fprintf(stderr, "Error! close share memory semaphore : %s\n", strerror(errno));
}

void freeResource()
{

	close(fd);

	closeSemaphore();

	sem_unlink(SHMNAME);
	sem_unlink(EMPTY);
	sem_unlink(FIRST);
	sem_unlink(SECOND);
	shm_unlink(SHMNAME);
}
void attachSHMemory()
{

	int fd;
	fd = shm_open(SHMNAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd == -1)
	{
		fprintf(stderr, "(shared memory open)Error! %s dont open because %s %d\n", SHMNAME, strerror(errno), errno);
		exit(EXIT_FAILURE);
	}

	if (ftruncate(fd, sizeof(sharedmemory)) == -1)
	{
		fprintf(stderr, "(ftruncate)Error! shared memory dont allocate because %s\n", strerror(errno));
		shm_unlink(SHMNAME);
		exit(EXIT_FAILURE);
	}

	share = mmap(NULL, sizeof(*share), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (share == MAP_FAILED)
	{
		fprintf(stderr, "(mmap)Error! dont mapping because %s\n", strerror(errno));
		shm_unlink(SHMNAME);
		exit(EXIT_FAILURE);
	}
}

void attachSHMSemaphore()
{
	if ((share_semaphore = sem_open(SHMNAME, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore SHM)Error! dont create semaphore because %s\n", strerror(errno));
		shm_unlink(SHMNAME);
		exit(EXIT_FAILURE);
	}
}

void attachEMPTYSemaphore(int bufSize)
{
	if ((empty_semaphore = sem_open(EMPTY, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, bufSize)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore EMPTY)Error! dont create semaphore because %s\n", strerror(errno));
		shm_unlink(EMPTY);
		exit(EXIT_FAILURE);
	}
}

void attachFIRSTSemaphore()
{
	if ((first_semaphore = sem_open(FIRST, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore FIRST)Error! dont create semaphore because %s\n", strerror(errno));
		shm_unlink(FIRST);
		exit(EXIT_FAILURE);
	}
}

void attachSECONDSemaphore()
{
	if ((second_semaphore = sem_open(SECOND, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore SECOND)Error! dont create semaphore because %s\n", strerror(errno));
		shm_unlink(SECOND);
		exit(EXIT_FAILURE);
	}
}

void checkArg(const char param, int value, int *arg)
{
	if ((optarg[0] == '0' || optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '3' || optarg[0] == '4' || optarg[0] == '5' || optarg[0] == '6' || optarg[0] == '7' || optarg[0] == '8' || optarg[0] == '9'))

		if (atoi(optarg) >= value)
		{
			*arg = atoi(optarg);
		}
		else
		{
			fprintf(stderr, "The -%c parameter must be >=%d . Program terminate\n", param, value);
			exit(EXIT_FAILURE);
		}
	else
	{
		fprintf(stderr, "The -%c parameter must be an unsigned integer. Program terminate\n", param);
		exit(EXIT_FAILURE);
	}
}

char getvaccine()
{
	char vaccine[1];
	ssize_t nread;
	nread = read(fd, vaccine, 1);
	if (nread == -1)
	{
		fprintf(stderr, "Read Error! : %s. Program terminate. %d\n", strerror(errno), getpid());
		sem_post(share_semaphore);
		sem_post(empty_semaphore);
		closeSemaphore();
		exit(EXIT_FAILURE);
	}
	else if (nread == 0)
	{
		vaccine[0] = '0';
	}
	else if (vaccine[0] != '1' && vaccine[0] != '2')
	{
		fprintf(stderr, "Read Error! Invalid character %c : %s. Program terminate. %d\n", vaccine[0], strerror(errno), getpid());
		sem_post(share_semaphore);
		sem_post(empty_semaphore);
		closeSemaphore();
		exit(EXIT_FAILURE);
	}

	return vaccine[0];
}

void Vaccinator(int order)
{
	signal(SIGINT, INTChildhandler);
	sem_wait(share_semaphore);
	share->vaccinators[order - 1].pid = getpid();
	sem_post(share_semaphore);
	while (1)
	{
		sem_wait(first_semaphore);
		sem_wait(second_semaphore);
		sem_wait(share_semaphore);

		if (share->vaccineEnd)
		{

			//for old to young vaccinate (count % t times)
			printf("Vaccinator %d (pid=%d) is inviting citizen pid=%d to the clinic\n", order, getpid(), share->citizens[share->vaccineCount % _carg].pid);
			write(share->citizens[share->vaccineCount % _carg].pipefd[1], "1", 1);
			share->firstShotBuffer--;
			share->secondShotBuffer--;
			share->citizens[share->vaccineCount % _carg].dozeCount++;

			if (share->citizens[share->vaccineCount % _carg].dozeCount == _targ)
			{
				share->totalVaccinedCitizen++;
				share->citizens[share->vaccineCount % _carg].remaining = _carg - share->totalVaccinedCitizen;
			}
			share->vaccineCount++;
			share->vaccinators[order - 1].dozeCount++;

			if (share->vaccineCount == (_targ * _carg))
			{
				share->vaccineEnd = 0;

				sem_post(share_semaphore);
				sem_post(empty_semaphore);
				sem_post(empty_semaphore);

				sem_post(second_semaphore); //inverse barrier. go to end. all vaccinator go terminate.
				sem_post(first_semaphore);
				break;
			}

			sem_post(share_semaphore);
			sem_post(empty_semaphore);
			sem_post(empty_semaphore);
		}
		else
		{
			sem_post(share_semaphore);
			sem_post(second_semaphore);
			sem_post(first_semaphore);
			break;
		}
	}
}

void Nurse(int order)
{
	signal(SIGINT, INTChildhandler);
	while (1)
	{
		sem_wait(empty_semaphore);
		sem_wait(share_semaphore);

		if (share->carryEnd)
		{

			char vac = getvaccine();
			if (vac == '0' || share->receiveVaccine == (2 * _carg * _targ))
			{
				share->carryEnd = 0;
				printf("Nurses have carried all vaccines to the buffer, terminating.\n");
				sem_post(share_semaphore);
				sem_post(empty_semaphore);
				break;
			}
			else if (vac == '1')
			{
				share->firstShotBuffer++;
				share->receiveVaccine++;
				printf("Nurse %d (pid=%d) has brought vaccine 1: the clinic has %d vaccine1 and %d vaccine2.\n", order, getpid(), share->firstShotBuffer, share->secondShotBuffer);
				sem_post(share_semaphore);
				sem_post(first_semaphore);
			}
			else if (vac == '2')
			{
				share->secondShotBuffer++;
				share->receiveVaccine++;
				printf("Nurse %d (pid=%d) has brought vaccine 2: the clinic has %d vaccine1 and %d vaccine2.\n", order, getpid(), share->firstShotBuffer, share->secondShotBuffer);
				sem_post(share_semaphore);
				sem_post(second_semaphore);
			}
		}
		else
		{
			sem_post(share_semaphore);
			sem_post(empty_semaphore);
			break;
		}
	}
}

void Citizen(int order)
{
	signal(SIGINT, INTChildhandler);
	int fd;
	sem_wait(share_semaphore);
	share->citizens[order - 1].pid = getpid();
	fd = share->citizens[order - 1].pipefd[0];
	sem_post(share_semaphore);

	char messsage[1];
	ssize_t nread;
	int dozeCount = 0;
	while (dozeCount < _targ)
	{
		nread = read(fd, messsage, 1); //wait get the message inviting
		if (nread == -1)
		{
			fprintf(stderr, "Read Error! : %s. Program terminate. %d\n", strerror(errno), getpid());
			exit(EXIT_FAILURE);
		}
		else if (nread == 0)
		{
			printf("hata\n");
		}
		dozeCount++;
		if (messsage[0] == '1')
		{
			sem_wait(share_semaphore);
			if (dozeCount == _targ)
				printf("Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2. The citizen is leaving. Remaining citizens to vaccinate: %d \n", order, getpid(), dozeCount, share->firstShotBuffer, share->secondShotBuffer, share->citizens[order - 1].remaining);
			else
				printf("Citizen %d (pid=%d) is vaccinated for the %d time: the clinic has %d vaccine1 and %d vaccine2\n", order, getpid(), dozeCount, share->firstShotBuffer, share->secondShotBuffer);
			sem_post(share_semaphore);
		}
	}
}

int isValidInput()
{
	char vaccine[1];
	ssize_t nread;
	int isValid = 0;
	for (int i = 0; i < _targ * _carg * 2; i++)
	{

		nread = read(fd, vaccine, 1);
		if (nread == -1)
		{
			fprintf(stderr, "Read Error! : %s. Program terminate. %d\n", strerror(errno), getpid());
			exit(EXIT_FAILURE);
		}
		else if (nread == 0)
		{
			isValid = -1;
			break;
		}
		else if (vaccine[0] == '1')
		{
			isValid++;
		}
		else if (vaccine[0] == '2')
		{
			isValid--;
		}
		else
		{
			isValid = -1;
			break;
		}
	}
	return isValid;
}

int main(int argc, char *argv[])
{
	signal(SIGINT, INThandler);
	int opt;
	setvbuf(stdout, NULL, _IONBF, 0);
	while ((opt = getopt(argc, argv, "n:v:c:b:t:i:")) != -1)
	{
		switch (opt)
		{

		case 'n':
			_nflag = 1;
			checkArg('n', 2, &_narg);
			break;

		case 'v':
			_vflag = 1;
			checkArg('v', 2, &_varg);
			if (_varg > SIZE)
			{
				fprintf(stderr, "Please change on code SIZE %d value because -v %d. Recompile the code after the change. Program terminate\n", SIZE, _varg);
				exit(EXIT_FAILURE);
			}
			break;

		case 'c':
			_cflag = 1;
			checkArg('c', 3, &_carg);
			if (_carg > SIZE)
			{
				fprintf(stderr, "Please change on code SIZE %d value because -c %d. Recompile the code after the change. Program terminate\n", SIZE, _carg);
				exit(EXIT_FAILURE);
			}
			break;

		case 'b':
			_bflag = 1;
			checkArg('b', 4, &_barg); //tc+1 min 4 (3*1)+1
			break;

		case 't':
			_tflag = 1;
			checkArg('t', 1, &_targ);
			break;

		case 'i':
			_iflag = 1;
			_iarg = optarg;

			break;

		case '?':
			if (optopt == 'n' || optopt == 'v' || optopt == 'c' || optopt == 'b' || optopt == 't' || optopt == 'i')
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

	if (!(_nflag == 1 && _vflag == 1 && _cflag == 1 && _bflag == 1 && _tflag == 1 && _iflag == 1))
	{
		fprintf(stderr, "You must use all parameter. -n -v -c -b -t -i. Program terminate\n");
		exit(EXIT_FAILURE);
	}

	for (; optind < argc; optind++)
	{
		fprintf(stderr, "Error extra parameter: '%s'. Program terminate\n", argv[optind]);
		exit(EXIT_FAILURE);
	}

	if (!(((_targ * _carg) + 1) <= _barg))
	{
		fprintf(stderr, "The -b parameter must be >=%d . Program terminate\n", ((_targ * _carg) + 1));
		exit(EXIT_FAILURE);
	}

	fd = open(_iarg, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", _iarg, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (isValidInput())
	{
		fprintf(stderr, "Error! Please check(1 and 2 count) or (invalid character) input file\n");
		close(fd);
		exit(EXIT_FAILURE);
	}

	lseek(fd, 0, SEEK_SET);

	attachSHMemory();
	attachSHMSemaphore();
	attachEMPTYSemaphore(_barg);
	attachFIRSTSemaphore();
	attachSECONDSemaphore();

	printf("Welcome to the GTU344 clinic. Number of citizen to vaccinate c=%d with t=%d doses.\n", _carg, _targ);

	share->vaccineCount = 0;
	share->citizenCount = _carg;
	share->firstShotBuffer = 0;
	share->secondShotBuffer = 0;
	share->receiveVaccine = 0;
	share->carryEnd = 1;
	share->vaccineEnd = 1;
	share->totalVaccinedCitizen = 0;

	//create pipes
	for (int i = 0; i < _carg; i++)
	{
		if (pipe(share->citizens[i].pipefd) == -1)
		{
			fprintf(stderr, "pipe() Error: %s Program terminate. Please use command (ulimit -n 4096)\n", strerror(errno));

			freeResource();

			exit(EXIT_FAILURE);
		}
	}

	//create citizen
	for (int i = 1; i <= _carg; ++i)
	{
		pid_t pid;
		if ((pid = fork()) < 0)
		{
			fprintf(stderr, "fork() Error. Program terminate\n");

			freeResource();

			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			Citizen(i);

			closeSemaphore();

			exit(0);
		}
	}

	//create vaccinator
	for (int i = 1; i <= _varg; ++i)
	{
		pid_t pid;
		if ((pid = fork()) < 0)
		{
			fprintf(stderr, "fork() Error. Program terminate\n");

			freeResource();

			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			Vaccinator(i);

			closeSemaphore();

			exit(0);
		}
	}

	//create nurse
	for (int i = 1; i <= _narg; ++i)
	{
		pid_t pid;
		if ((pid = fork()) < 0)
		{
			fprintf(stderr, "fork() Error. Program terminate\n");

			freeResource();

			exit(EXIT_FAILURE);
		}
		else if (pid == 0)
		{
			Nurse(i);

			closeSemaphore();

			exit(0);
		}
	}

	/* Wait for children to exit. */
	int status;
	int n = _narg + _varg + _carg;
	while (n > 0)
	{
		wait(&status);

		--n;
	}
	printf("All citizens have been vaccinated.\n");
	for (int i = 0; i < _varg; i++)
	{
		if (i + 1 == _varg)
			printf("Vaccinator %d (pid=%d) vaccinated %d doses. The clinic is now closed. Stay healthy.\n", i + 1, share->vaccinators[i].pid, share->vaccinators[i].dozeCount);
		else
			printf("Vaccinator %d (pid=%d) vaccinated %d doses. ", i + 1, share->vaccinators[i].pid, share->vaccinators[i].dozeCount);
	}

	freeResource();

	return 0;
}