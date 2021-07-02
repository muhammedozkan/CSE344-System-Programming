/*---------------------------------------------//
//Gebze Technical University                   //
//CSE344 Systems Programming Course            //
//Muhammed OZKAN 151044084                     //
//---------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

#define SIZE 8192
#define QUEUESIZE 7

#define FULLSEM "151044084fullsem"
#define EMPTYSEM "151044084emptysem"
#define RESPONSE "151044084responsesem"
#define REQUEST "151044084requestsem"
#define SHSEM "151044084sharesem"
#define SALT "151044084"

typedef struct student
{
	int id;
	char name[NAME_MAX];
	char semName[NAME_MAX];
	int quality;
	int speed;
	int cost;
	int active;
	int earnMoney;
	int count;
} student;

student students[SIZE];

sem_t *student_semaphore[SIZE];
pthread_t students_tid[SIZE];

sem_t *threadH_semaphore = NULL;
pthread_t threadH;

sem_t *main_semaphore = NULL;
sem_t *response_semaphore = NULL;
sem_t *request_semaphore = NULL;

sem_t *share_semaphore = NULL;
char queue[QUEUESIZE];
int first = 0;
int last = QUEUESIZE - 1;

char sendmessage;
int fd;
FILE *fptr;
int money = 0;
int studentCount = 0;
int noHomework = 0;

int isExit = 0;

int compareID(const void *s1, const void *s2)
{
	student *e1 = (student *)s1;
	student *e2 = (student *)s2;

	if (e1->id < e2->id)
	{
		return -1;
	}
	else if (e1->id > e2->id)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int compareCost(const void *s1, const void *s2)
{
	student *e1 = (student *)s1;
	student *e2 = (student *)s2;

	if (e1->cost < e2->cost)
	{
		return -1;
	}
	else if (e1->cost > e2->cost)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int compareSpeed(const void *s1, const void *s2)
{
	student *e1 = (student *)s1;
	student *e2 = (student *)s2;

	if (e1->speed < e2->speed)
	{
		return 1;
	}
	else if (e1->speed > e2->speed)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

int compareQuality(const void *s1, const void *s2)
{
	student *e1 = (student *)s1;
	student *e2 = (student *)s2;

	if (e1->quality < e2->quality)
	{
		return 1;
	}
	else if (e1->quality > e2->quality)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

void closeSemaphore()
{
	if (sem_close(share_semaphore) != 0)
		fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", SHSEM, strerror(errno));
	if (sem_unlink(SHSEM) != 0)
		fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", SHSEM, strerror(errno));
}

void freeResource()
{
	for (int i = 0; i < studentCount; i++)
	{
		if (sem_close(student_semaphore[i]) != 0)
			fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", students[i].semName, strerror(errno));
		if (sem_unlink(students[i].semName) != 0)
			fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", students[i].semName, strerror(errno));
	}

	if (sem_close(threadH_semaphore) != 0)
		fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", EMPTYSEM, strerror(errno));
	if (sem_unlink(EMPTYSEM) != 0)
		fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", EMPTYSEM, strerror(errno));

	if (sem_close(main_semaphore) != 0)
		fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", FULLSEM, strerror(errno));
	if (sem_unlink(FULLSEM) != 0)
		fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", FULLSEM, strerror(errno));

	if (sem_close(response_semaphore) != 0)
		fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", RESPONSE, strerror(errno));
	if (sem_unlink(RESPONSE) != 0)
		fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", RESPONSE, strerror(errno));

	if (sem_close(request_semaphore) != 0)
		fprintf(stderr, "(semaphore)Error! close %s semaphore : %s\n", REQUEST, strerror(errno));
	if (sem_unlink(REQUEST) != 0)
		fprintf(stderr, "(semaphore)Error! unlink %s semaphore : %s\n", REQUEST, strerror(errno));

	closeSemaphore();
	fclose(fptr);
	close(fd);
}

void attachSHMSemaphore()
{
	if ((share_semaphore = sem_open(SHSEM, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", SHSEM, strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}
}

char getHomework()
{
	char homework[1];
	ssize_t nread;
	nread = read(fd, homework, 1);
	if (nread == -1)
	{
		fprintf(stderr, "Read Error! : %s. Program terminate.\n", strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}
	else if (nread == 0)
	{
		homework[0] = '0';
	}
	else if (homework[0] == '\n' || homework[0] == '\r')
	{
		homework[0] = '0';
	}
	else if (homework[0] != 'C' && homework[0] != 'S' && homework[0] != 'Q')
	{
		fprintf(stderr, "\n\t\t\tRead Error! Invalid character '%c'. Program terminate.\n\n", homework[0]);
		homework[0] = '0';
	}

	return homework[0];
}

char *readStudent(FILE *_fptr, int row)
{
	rewind(_fptr);
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, _fptr)) != -1)
	{
		if (count == row)
		{
			return line;
		}

		count++;
	}

	return line;
}

int getStudentCount(char *fileName)
{
	FILE *_fptr = NULL;

	if ((_fptr = fopen(fileName, "r")) == NULL)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", fileName, strerror(errno));
		close(fd);
		fclose(fptr);
		exit(EXIT_FAILURE);
	}

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, _fptr)) != -1)
	{
		count++;
	}
	free(line);

	fclose(_fptr);

	return count;
}

void parseDouble(char *line, student *data, int id)
{
	const char s[2] = " ";
	char *token;
	int i = 0;
	token = strtok(line, s);

	data->id = id;
	while (token != NULL)
	{

		if (i == 0)
		{
			strcpy(data->name, token);
		}
		else if (i == 1)
		{
			data->quality = atoi(token);

			if (!(data->quality >= 1 && data->quality <= 5))
			{
				fprintf(stderr, "%s student's quality must be greater than 1 and less than 5. Program terminate.\n", data->name);
				close(fd);
				fclose(fptr);
				exit(EXIT_FAILURE);
			}
		}
		else if (i == 2)
		{
			data->speed = atoi(token);

			if (!(data->speed >= 1 && data->speed <= 5))
			{
				fprintf(stderr, "%s student's speed must be greater than 1 and less than 5. Program terminate.\n", data->name);
				close(fd);
				fclose(fptr);
				exit(EXIT_FAILURE);
			}
		}
		else if (i == 3)
		{

			data->cost = atoi(token);

			if (!(data->cost >= 100 && data->cost <= 1000))
			{
				fprintf(stderr, "%s student's money must be greater than 100 and less than 1000. Program terminate.\n", data->name);
				close(fd);
				fclose(fptr);
				exit(EXIT_FAILURE);
			}
		}

		i++;

		token = strtok(NULL, s);
	}
}

void *threadHThread(void *arg)
{
	char tmp;
	do
	{
		sem_wait(threadH_semaphore);
		if (isExit)
		{
			break;
		}
		if ((sendmessage == '0'))
		{
			printf("H has no more money for homework on current student, terminating.\n");
			break;
		}
		sem_wait(share_semaphore);
		if (isExit)
		{
			sem_post(share_semaphore);
			break;
		}
		tmp = getHomework();
		if ((tmp != '0'))
		{
			if (money == 0)
			{
				last = (last + 1) % QUEUESIZE;
				queue[last] = '0';
				tmp = '0';
				printf("H has no more money for homeworks, terminating.\n");
			}
			else
			{
				last = (last + 1) % QUEUESIZE;
				queue[last] = tmp;

				printf("H has a new homework %c; remaining money is %dTL\n", tmp, money);
			}
		}
		else
		{
			noHomework = 1;

			last = (last + 1) % QUEUESIZE;
			queue[last] = tmp;

			printf("H has no other homeworks, terminating.\n");
		}

		sem_post(share_semaphore);
		sem_post(main_semaphore);
	} while ((tmp != '0'));
	pthread_detach(threadH);
	return NULL;
}

void *studentThread(void *arg)
{
	int id = *((int *)(arg));
	char name[NAME_MAX];
	int speed;
	int cost;

	sem_wait(share_semaphore);
	if (isExit)
	{
		sem_post(share_semaphore);
		pthread_detach(students_tid[id]);
		return NULL;
	}
	for (int i = 0; i < studentCount; i++)
	{
		if (students[i].id == id)
		{
			strcpy(name, students[i].name);

			speed = students[i].speed;
			cost = students[i].cost;
			break;
		}
	}
	sem_post(share_semaphore);
	if (isExit)
	{
		pthread_detach(students_tid[id]);
		return NULL;
	}
	while (1)
	{

		printf("%s is waiting for a homework\n", name);
		sem_post(request_semaphore);
		sem_wait(student_semaphore[id]);
		if (isExit)
		{
			pthread_detach(students_tid[id]);
			break;
		}

		sem_wait(share_semaphore);
		if (isExit)
		{
			sem_post(share_semaphore);
			pthread_detach(students_tid[id]);
			break;
		}
		if ((sendmessage == '0'))
		{
			sem_post(share_semaphore);
			break;
		}

		printf("%s is solving homework %c for %d, H has %dTL left.\n", name, sendmessage, cost, money);

		sem_post(share_semaphore);
		sem_post(response_semaphore);
		if (isExit)
		{
			pthread_detach(students_tid[id]);
			break;
		}

		for (int i = 0; i < (6 - speed) * 5; i++) //this loop to check the Output value as soon as CTRL-C signal comes. checks CTRL-C signal 5 times per second
		{
			usleep(200000); //for simulation 1000microsecond * 1000 = 1second
			if (isExit)
			{
				pthread_detach(students_tid[id]);
				break;
			}
		}

		if (isExit)
		{
			pthread_detach(students_tid[id]);
			break;
		}
		sem_wait(share_semaphore);
		if (isExit)
		{
			sem_post(share_semaphore);
			pthread_detach(students_tid[id]);
			break;
		}
		for (int i = 0; i < studentCount; i++)
		{
			if (students[i].id == id)
			{
				students[i].active = 0;
				break;
			}
		}
		sem_post(share_semaphore);
	}

	return NULL;
}

//for CTRL-C signal catch
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	isExit = 1;

	sem_post(threadH_semaphore);
	for (int i = 0; i < studentCount; i++)
	{
		sem_post(student_semaphore[i]);
		sem_post(student_semaphore[i]);
	}

	char error[56];
	strcpy(error, "\nProgram terminated because CTRL-C keys were pressed.\n");
	write(fileno(stderr), error, strlen(error));

	sem_wait(share_semaphore);
	printf("Termination signal received, closing.\n");
	qsort(students, studentCount, sizeof(student), compareID);
	printf("Homeworks solved and money made by the students:\n");

	int totalCount = 0;
	int totalMoney = 0;

	for (int i = 0; i < studentCount; i++)
	{
		printf("%-20s\t %d\t %d\n", students[i].name, students[i].count, students[i].earnMoney);
		totalCount += students[i].count;
		totalMoney += students[i].earnMoney;
	}

	printf("Total cost for %d homeworks %dTL\n", totalCount, totalMoney);
	printf("Money left at H’s account: %dTL\n", money);

	sem_post(share_semaphore);

	freeResource();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	signal(SIGINT, INThandler);
	//setvbuf(stdout, NULL, _IONBF, 0);

	if (argc != 4)
	{
		fprintf(stderr, "It should run it like this. For example /program homeworkFilePath studentsFilePath 10000. Program terminate.\n");
		exit(EXIT_FAILURE);
	}

	if ((fd = open(argv[1], O_RDONLY)) == -1)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", argv[1], strerror(errno));
		exit(EXIT_FAILURE);
	}

	if ((fptr = fopen(argv[2], "r")) == NULL)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", argv[2], strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}

	if ((argv[3][0] == '0' || argv[3][0] == '1' || argv[3][0] == '2' || argv[3][0] == '3' || argv[3][0] == '4' || argv[3][0] == '5' || argv[3][0] == '6' || argv[3][0] == '7' || argv[3][0] == '8' || argv[3][0] == '9'))
	{
		money = atoi(argv[3]);
	}
	else
	{
		fprintf(stderr, "The money must be an unsigned integer. Program terminate\n");
		close(fd);
		fclose(fptr);
		exit(EXIT_FAILURE);
	}
	studentCount = getStudentCount(argv[2]);

	if (!(studentCount > 0))
	{
		fprintf(stderr, "The students %s file, the number of students must be greater than one. Program terminate\n", argv[2]);
		close(fd);
		fclose(fptr);
		exit(EXIT_FAILURE);
	}

	if (studentCount > SIZE)
	{
		fprintf(stderr, "Please change on code SIZE %d value because students size %d. Recompile the code after the change. Program terminate\n", SIZE, studentCount);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < studentCount; i++)
	{
		student tmpstd;
		char *tmp;
		tmp = readStudent(fptr, i);

		if (tmp[0] == '\r' || tmp[0] == '\n')
		{
			fprintf(stderr, "Please check %s file. Contains blank lines. Program terminate\n", argv[2]);
			close(fd);
			fclose(fptr);
			exit(EXIT_FAILURE);
		}

		parseDouble(tmp, &tmpstd, i);
		free(tmp);
		students[i].id = tmpstd.id;
		strcpy(students[i].name, tmpstd.name);
		strcpy(students[i].semName, tmpstd.name);
		strcat(students[i].semName, SALT);
		students[i].quality = tmpstd.quality;
		students[i].speed = tmpstd.speed;
		students[i].cost = tmpstd.cost;
		students[i].active = 0;
		students[i].earnMoney = 0;
		students[i].count = 0;

		for (int j = 0; j < i; j++)
		{
			if (strcmp(students[i].name, students[j].name) == 0)
			{
				fprintf(stderr, "Please check %s file. Contains duplicate student names. Program terminate\n", argv[2]);
				close(fd);
				fclose(fptr);
				exit(EXIT_FAILURE);
			}
		}
	}

	printf("%d students-for-hire threads have been created.\n\tName\t\t Q\t S\t C\n", studentCount);
	for (int i = 0; i < studentCount; i++)
	{
		printf("%-20s\t %d\t %d\t %d\n", students[i].name, students[i].quality, students[i].speed, students[i].cost);
	}

	attachSHMSemaphore();

	if ((threadH_semaphore = sem_open(EMPTYSEM, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, QUEUESIZE)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", EMPTYSEM, strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}

	if ((main_semaphore = sem_open(FULLSEM, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", FULLSEM, strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}

	if ((response_semaphore = sem_open(RESPONSE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", RESPONSE, strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}

	if ((request_semaphore = sem_open(REQUEST, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", REQUEST, strerror(errno));
		freeResource();
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < studentCount; i++)
	{
		if ((student_semaphore[i] = sem_open(students[i].semName, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
		{
			fprintf(stderr, "(semaphore)Error! dont create %s semaphore : %s\n", students[i].semName, strerror(errno));
			freeResource();
			exit(EXIT_FAILURE);
		}
	}

	int error;
	for (int i = 0; i < studentCount; i++)
	{
		if ((error = pthread_create(&students_tid[i], NULL, studentThread, &students[i].id)))
		{
			fprintf(stderr, "Failed to create Student thread %s: %s\n", students[i].name, strerror(error));
			freeResource();
			exit(EXIT_FAILURE);
		}
	}

	if ((error = pthread_create(&threadH, NULL, threadHThread, NULL)))
	{
		fprintf(stderr, "Failed to create threadH: %s\n", strerror(error));
		freeResource();
		exit(EXIT_FAILURE);
	}
	int flag;
	while (1) //main thread
	{
		flag = 1;
		char tmp;

		sem_wait(main_semaphore);
		sem_wait(share_semaphore);

		if ((queue[first] != '0'))
		{
			tmp = queue[first];
			first = (first + 1) % QUEUESIZE;

			if ((tmp == 'Q'))
			{
				qsort(students, studentCount, sizeof(student), compareQuality);
			}
			else if ((tmp == 'S'))
			{
				qsort(students, studentCount, sizeof(student), compareSpeed);
			}
			else if ((tmp == 'C'))
			{
				qsort(students, studentCount, sizeof(student), compareCost);
			}
			sem_post(share_semaphore);

			sem_wait(request_semaphore);

			sem_wait(share_semaphore);
			int i;
			for (i = 0; i < studentCount; i++)
			{

				if (students[i].active == 0)
				{
					if (money >= students[i].cost)
					{
						flag = 1;
						break;
					}
					else
					{
						flag = 0;
					}
				}
			}

			if (flag)
			{
				students[i].active = 1;
				students[i].earnMoney += students[i].cost;
				students[i].count++;
				money -= students[i].cost;
				sendmessage = tmp;
				sem_post(student_semaphore[students[i].id]);
				sem_post(share_semaphore);
				sem_wait(response_semaphore);
			}
			else
			{
				sendmessage = '0';
				sem_post(share_semaphore);
				break;
			}
		}
		else
		{
			sendmessage = '0';
			sem_post(share_semaphore);
			break;
		}
		sem_post(threadH_semaphore);
	}

	sem_post(threadH_semaphore);
	for (int i = 0; i < studentCount; i++)
	{
		sem_post(student_semaphore[i]);
		sem_post(student_semaphore[i]);
	}

	pthread_join(threadH, NULL);

	for (int i = 0; i < studentCount; i++)
	{
		pthread_join(students_tid[i], NULL);
	}

	if (money != 0 && noHomework == 0)
	{
		printf("Money is over, closing.\n");
	}
	else if (money != 0 && noHomework == 1 && flag == 1)
	{
		printf("No more homeworks left or coming in, closing.\n");
	}
	else if (money != 0 && noHomework == 1 && flag == 0)
	{
		printf("Money is over, closing.\n");
	}
	else
	{
		printf("Money is over, closing.\n");
	}

	qsort(students, studentCount, sizeof(student), compareID);
	printf("Homeworks solved and money made by the students:\n");

	int totalCount = 0;
	int totalMoney = 0;

	for (int i = 0; i < studentCount; i++)
	{
		printf("%-20s\t %d\t %d\n", students[i].name, students[i].count, students[i].earnMoney);
		totalCount += students[i].count;
		totalMoney += students[i].earnMoney;
	}

	printf("Total cost for %d homeworks %dTL\n", totalCount, totalMoney);
	printf("Money left at H’s account: %dTL\n", money);

	freeResource();

	return 0;
}