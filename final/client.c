/*---------------------------------------------//
//Gebze Technical University                   //
//CSE344 Systems Programming Course            //
//Muhammed OZKAN 151044084                     //
//---------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>

#define BUFFSIZE 4096

int _iflag = 0, _aflag = 0, _pflag = 0, _oflag = 0;
int _iarg = 0, _parg = 0;
char *_aarg = NULL, *_oarg = NULL;
FILE *fptr = NULL;
int totalquery = 0;
char readbuff[BUFFSIZE];
char writebuff[BUFFSIZE];
int sockfd;
struct sockaddr_in servaddr;
struct timeval tv1, tv2, diff;

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

char *readQuery(FILE *fptr, int row)
{

	rewind(fptr);

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, fptr)) != -1)
	{
		if (count == row)
		{
			return line;
		}

		count++;
	}

	return line;
}

int getQueryCount(FILE *fptr)
{

	rewind(fptr);

	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	int count = 0;

	while ((read = getline(&line, &len, fptr)) != -1)
	{
		count++;
	}
	free(line);

	return count;
}

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	struct timeval xx = *x;
	struct timeval yy = *y;
	x = &xx;
	y = &yy;

	if (x->tv_usec > 999999)
	{
		x->tv_sec += x->tv_usec / 1000000;
		x->tv_usec %= 1000000;
	}

	if (y->tv_usec > 999999)
	{
		y->tv_sec += y->tv_usec / 1000000;
		y->tv_usec %= 1000000;
	}

	result->tv_sec = x->tv_sec - y->tv_sec;

	if ((result->tv_usec = x->tv_usec - y->tv_usec) < 0)
	{
		result->tv_usec += 1000000;
		result->tv_sec--;
	}

	return result->tv_sec < 0;
}

char *time_stamp()
{

	char *timestamp = (char *)malloc(sizeof(char) * 16);
	time_t ltime;
	ltime = time(NULL);
	struct tm *tm;
	tm = localtime(&ltime);

	sprintf(timestamp, "%04d%02d%02d%02d%02d%02d", tm->tm_year + 1900, tm->tm_mon + 1,
			tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	return timestamp;
}
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
}
int main(int argc, char *argv[])
{
	signal(SIGINT, INThandler);
	int opt;

	while ((opt = getopt(argc, argv, "i:a:p:o:")) != -1)
	{
		switch (opt)
		{

		case 'i':
			_iflag = 1;
			checkArg('i', 1, &_iarg);
			break;

		case 'a':
			_aflag = 1;
			_aarg = optarg;
			break;

		case 'p':
			_pflag = 1;
			checkArg('p', 1001, &_parg);
			break;

		case 'o':
			_oflag = 1;
			_oarg = optarg;
			break;

		case '?':
			if (optopt == 'i' || optopt == 'a' || optopt == 'p' || optopt == 'o')
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

	if (!(_iflag == 1 && _aflag == 1 && _pflag == 1 && _oflag == 1))
	{
		fprintf(stderr, "You must use all parameter. -i -a -p -o. Program terminate\n");
		exit(EXIT_FAILURE);
	}

	for (; optind < argc; optind++)
	{
		fprintf(stderr, "Error extra parameter: '%s'. Program terminate\n", argv[optind]);
		exit(EXIT_FAILURE);
	}

	if ((fptr = fopen(_oarg, "r")) == NULL)
	{
		fprintf(stderr, "Error! %s dont opening because %s\n", _oarg, strerror(errno));
		fclose(fptr);
		exit(EXIT_FAILURE);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		fprintf(stderr, "Error! Socket creation failed %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(_aarg);
	servaddr.sin_port = htons(_parg);

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
	{
		fprintf(stderr, "Error! Connection with the server failed %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	char *stamp = time_stamp();
	printf("%s - Client-%d connecting to %s:%d\n", stamp, _iarg, _aarg, _parg);
	free(stamp);

	int f = getQueryCount(fptr);

	char *tmp;
	int id;
	char query[4096];
	for (int i = 0; i < f; i++)
	{
		tmp = readQuery(fptr, i);
		sscanf(tmp, "%d %[^;\n]s", &id, query);
		free(tmp);

		if (id == _iarg)
		{

			bzero(writebuff, BUFFSIZE);
			strcpy(writebuff, "@NEWQUERY");
			write(sockfd, writebuff, BUFFSIZE);

			bzero(writebuff, BUFFSIZE);
			strcpy(writebuff, query);
			char *stamp = time_stamp();

			printf("%s - Client-%d connected and sending query '%s;'\n", stamp, id, query);
			free(stamp);
			totalquery++;
			write(sockfd, writebuff, BUFFSIZE);
			char resulttmp[4096];
			int totalresult = 0;
			gettimeofday(&tv1, NULL);
			while (1)
			{
				bzero(readbuff, BUFFSIZE);
				if (read(sockfd, readbuff, BUFFSIZE) <= 0)
				{
					break;
				}

				if (strncmp("@ENDRESULT", readbuff, 10) == 0)
				{

					break;
				}
				else
				{
					if (totalresult == 0)
					{
						strcpy(resulttmp, readbuff);
						printf("    %s\n", readbuff);
					}
					else
					{
						printf("%d- %s\n", totalresult, readbuff);
					}

					totalresult++;
				}
			}
			gettimeofday(&tv2, NULL);

			timeval_subtract(&diff, &tv2, &tv1);
			if (strncmp(resulttmp, "-(", 2) == 0)
			{
				char *stamp = time_stamp();

				printf("%s - Server’s response to Client-%d arrived in %ld.%ld seconds. \n", stamp, id, diff.tv_sec, diff.tv_usec);
				free(stamp);
			}
			else
			{
				char *stamp = time_stamp();

				printf("%s - Server’s response to Client-%d is %d records, and arrived in %ld.%ld seconds. \n", stamp, id, totalresult - 1, diff.tv_sec, diff.tv_usec);
				free(stamp);
			}
		}
	}

	bzero(writebuff, BUFFSIZE);
	strcpy(writebuff, "@NOQUERY");

	write(sockfd, writebuff, BUFFSIZE);
	stamp = time_stamp();
	printf("%s - A total of %d queries were executed, client is terminating.\n", stamp, totalquery);
	free(stamp);
	close(sockfd);

	fclose(fptr);
	return 0;
}