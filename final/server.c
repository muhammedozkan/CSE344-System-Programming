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
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <semaphore.h> //for run only one instance of server program.
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/stat.h>

#define SERVER_NAME "151044084serverprogramlock"
#define SIZE 1024
#define BUFFSIZE 4096

typedef struct CSV_FIELD
{
	char *text;
	size_t length;
} CSV_FIELD;

typedef struct CSV_BUFFER
{
	CSV_FIELD ***field;
	size_t rows;
	size_t *width;
	char field_delim;
	char text_delim;
} CSV_BUFFER;

typedef struct QUERYTHREAD
{
	pthread_t queryThread_tid;
	int queryThread_id;
	pthread_cond_t cond;
	int socketfd;
	int isReady;
	char readbuff[BUFFSIZE];
	char writebuff[BUFFSIZE];
} QUERYTHREAD;

int _pflag = 0, _oflag = 0, _lflag = 0, _dflag = 0;
int _parg = 0, _larg = 0;
char *_oarg = NULL, *_darg = NULL;

sem_t *run_lock = NULL;
FILE *fplogger = NULL;

int sockfd, connfd;
socklen_t len;
struct sockaddr_in servaddr, cli;

int availableThread;

struct timeval tv1, tv2, diff;

int intsignal = 0;

QUERYTHREAD threadpool[SIZE];
pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t available_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t response_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t response_mutex = PTHREAD_MUTEX_INITIALIZER;

int AR = 0;
int AW = 0;
int WR = 0;
int WW = 0;

pthread_cond_t okToRead = PTHREAD_COND_INITIALIZER;
pthread_cond_t okToWrite = PTHREAD_COND_INITIALIZER;

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

CSV_BUFFER *my_buffer;

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

void writeLog(const char *fmt, ...)
{
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	char *stamp = time_stamp();
	fprintf(fplogger, "%s - %s", stamp, buffer);
	free(stamp);
	//fflush(fplogger);
}

static int add_char(char **string, int *c, char ch)
{
	char *tmp = NULL;
	(*c)++;
	tmp = realloc(*string, (*c) + 1);
	if (tmp == NULL)
		return 1;
	*string = tmp;
	(*string)[(*c) - 1] = ch;
	(*string)[*c] = '\0';

	return 0;
}
static int set_field(CSV_FIELD *field, char *text)
{

	char *tmp;

	field->length = strlen(text) + 1;
	tmp = realloc(field->text, field->length);
	if (tmp == NULL)
		return 1;
	field->text = tmp;
	strcpy(field->text, text);

	return 0;
}

static CSV_FIELD *create_field()
{
	CSV_FIELD *field = malloc(sizeof(CSV_FIELD));
	field->length = 0;
	field->text = NULL;
	set_field(field, "\0");
	return field;
}

static void destroy_field(CSV_FIELD *field)
{
	if (field->text != NULL)
	{
		free(field->text);
		field->text = NULL;
	}
	free(field);
	field = NULL;
}

CSV_BUFFER *csv_create_buffer()
{

	CSV_BUFFER *buffer = malloc(sizeof(CSV_BUFFER));

	if (buffer != NULL)
	{
		buffer->field = NULL;
		buffer->rows = 0;
		buffer->width = NULL;
		buffer->field_delim = ',';
		buffer->text_delim = '"';
	}

	return buffer;
}

void csv_destroy_buffer(CSV_BUFFER *buffer)
{

	int i, j;

	for (i = 0; i < buffer->rows; i++)
	{
		for (j = 0; j < buffer->width[i]; j++)
		{
			destroy_field(buffer->field[i][j]);
		}
		free(buffer->field[i]);
		buffer->field[i] = NULL;
	}

	if (buffer->field != NULL)
		free(buffer->field);

	if (buffer->width != NULL)
		free(buffer->width);

	free(buffer);
}

static int append_field(CSV_BUFFER *buffer, size_t row)
{

	CSV_FIELD **temp_field;

	if (buffer->rows < row + 1)
		return 1;

	int col = buffer->width[row];

	temp_field = realloc(buffer->field[row],
						 (col + 1) * sizeof(CSV_FIELD *));
	if (temp_field == NULL)
	{
		return 2;
	}
	else
	{
		buffer->field[row] = temp_field;
		buffer->field[row][col] = create_field();
		buffer->width[row]++;
	}

	return 0;
}

static int append_row(CSV_BUFFER *buffer)
{
	size_t *temp_width;
	CSV_FIELD ***temp_field;

	size_t row = buffer->rows;

	temp_width = realloc(buffer->width, (buffer->rows + 1) *
											sizeof(size_t));
	if (temp_width != NULL)
	{
		buffer->width = temp_width;
		buffer->width[row] = 0;
	}
	else
	{
		return 1;
	}

	temp_field = realloc(buffer->field, (buffer->rows + 1) *
											sizeof(CSV_FIELD **));
	if (temp_field != NULL)
	{
		buffer->field = temp_field;
		buffer->field[row] = NULL;
	}
	else
	{
		free(temp_width);
		return 2;
	}

	buffer->rows++;
	append_field(buffer, row);
	return 0;
}

static int read_next_field(FILE *fp,
						   char field_delim, char text_delim,
						   CSV_FIELD *field)
{

	char ch = 'a';

	int done = 0;
	int in_text = 0;
	int esc = 0;

	int c = 0;
	char *tmp = malloc(1);
	tmp[0] = '\0';
	while (!done)
	{
		ch = getc(fp);

		if (ch == EOF)
		{
			c = 0;
			done = 1;
		}
		else if (!in_text)
		{
			if (ch == text_delim)
			{
				in_text = 1;
				c = 0;
			}
			else if (ch == field_delim)
			{
				done = 1;
			}
			else if (ch == '\n' || ch == '\r')
			{
				if (ch == '\r')
					ch = getc(fp);
				done = 1;
			}
			else
			{
				add_char(&tmp, &c, ch);
			}
		}
		else
		{
			if (esc)
			{
				if (ch == text_delim)
				{
					add_char(&tmp, &c, ch);
					esc = 0;
				}
				else
				{
					esc = 0;
					done = 1;
				}
			}
			else
			{
				if (ch == text_delim)
				{
					esc = 1;
				}
				else if (ch == field_delim)
				{
					add_char(&tmp, &c, ch);
				}
				else
				{
					add_char(&tmp, &c, ch);
				}
			}
		}
	}
	if (field != NULL)
	{
		set_field(field, tmp);
	}

	if (tmp != NULL)
		free(tmp);
	tmp = NULL;

	fpos_t pos;
	int retval;
	done = 0;
	while (!done)
	{
		if (ch == field_delim)
		{
			retval = 0;
			done = 1;
		}
		else if (ch == '\n' || ch == '\r')
		{
			if (ch == '\r')
				ch = getc(fp);
			fgetpos(fp, &pos);
			ch = getc(fp);
			if (ch == EOF)
				retval = 2;
			else
				retval = 1;
			fsetpos(fp, &pos);
			done = 1;
		}
		else if (ch == EOF)
		{
			retval = 2;
			done = 1;
		}
		else
		{
			ch = getc(fp);
		}
	}

	return retval;
}

int csv_load(CSV_BUFFER *buffer, char *file_name)
{

	FILE *fp = fopen(file_name, "r");
	if (fp == NULL)
		exit(EXIT_FAILURE);

	int next = 1;
	int end = 0;
	int first = 1;
	int i = -1, j = -1;

	while (!end)
	{

		if (!first)
		{
			next = read_next_field(fp,
								   buffer->field_delim, buffer->text_delim,
								   buffer->field[i][j - 1]);
		}

		if (next == 2)
			end = 1;

		if (next == 1)
		{
			if (append_row(buffer) != 0)
				return 2;
			j = 1;
			i++;
		}

		if (next == 0)
		{
			if (append_field(buffer, i) != 0)
				return 2;
			j++;
		}

		if (first)
			first = 0;
	}

	fclose(fp);
	return 0;
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

void make_daemon()
{
	pid_t pid;

	pid = fork();

	if (pid < 0)
		exit(EXIT_FAILURE);

	if (pid > 0)
		exit(EXIT_SUCCESS);

	if (setsid() < 0)
		exit(EXIT_FAILURE);

	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	pid = fork();

	if (pid < 0)
		exit(EXIT_FAILURE);

	if (pid > 0)
		exit(EXIT_SUCCESS);

	umask(0);

	chdir("/");

	int x;
	for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
	{
		close(x);
	}

	fplogger = fopen(_oarg, "a");
	setvbuf(fplogger, NULL, _IONBF, 0);
	writeLog("Executing with parameters:\n\t-p %d\n\t-o %s\n\t-l %d\n\t-d %s\n", _parg, _oarg, _larg, _darg);
}

char *ltrim(char *s)
{
	while (isspace(*s))
		s++;
	return s;
}

char *rtrim(char *s)
{
	char *back = s + strlen(s);
	while (isspace(*--back))
		;
	*(back + 1) = '\0';
	return s;
}

char *trim(char *s)
{
	return rtrim(ltrim(s));
}

void *queryThread(void *arg)
{
	int id = *((int *)(arg));

	while (1)
	{
		if (intsignal)
		{
			pthread_mutex_unlock(&lock_mutex);
			bzero(threadpool[id].writebuff, BUFFSIZE);
			strcpy(threadpool[id].writebuff, "@ENDRESULT");
			write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
			return NULL;
		}

		pthread_mutex_lock(&lock_mutex);
		threadpool[id].socketfd = -1;
		threadpool[id].isReady = 1;
		availableThread++;
		writeLog("Thread #%d: waiting for connection\n", id);
		pthread_cond_signal(&available_cond);
		pthread_cond_wait(&threadpool[id].cond, &lock_mutex);
		if (intsignal)
		{
			pthread_mutex_unlock(&lock_mutex);
			bzero(threadpool[id].readbuff, BUFFSIZE);
			read(threadpool[id].socketfd, threadpool[id].readbuff, BUFFSIZE);

			bzero(threadpool[id].readbuff, BUFFSIZE);
			read(threadpool[id].socketfd, threadpool[id].readbuff, BUFFSIZE);

			bzero(threadpool[id].writebuff, BUFFSIZE);
			strcpy(threadpool[id].writebuff, "@ENDRESULT");
			write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
			close(threadpool[id].socketfd);
			return NULL;
		}
		threadpool[id].isReady = 0;
		availableThread--;
		pthread_cond_signal(&response_cond);
		pthread_mutex_unlock(&lock_mutex);

		usleep(500000); //0.5 second
		int totalrecord = 0;
		while (1)
		{

			bzero(threadpool[id].readbuff, BUFFSIZE);
			read(threadpool[id].socketfd, threadpool[id].readbuff, BUFFSIZE);

			if (strncmp("@NOQUERY", threadpool[id].readbuff, 8) == 0)
			{

				if (totalrecord == 0)
				{
					writeLog("Thread #%d: There is no query. 0 records have been returned.\n", id);
				}

				break;
			}
			else if (strncmp("@NEWQUERY", threadpool[id].readbuff, 9) == 0)
			{
				totalrecord = 0;

				bzero(threadpool[id].readbuff, BUFFSIZE);
				read(threadpool[id].socketfd, threadpool[id].readbuff, BUFFSIZE);

				writeLog("Thread #%d: received query '%s;'\n", id, threadpool[id].readbuff);

				if (strncmp("SELECT", threadpool[id].readbuff, 6) == 0)
				{
					pthread_mutex_lock(&m);
					while ((AW + WW) > 0)
					{
						WR++;
						pthread_cond_wait(&okToRead, &m);
						WR--;
					}
					AR++;
					pthread_mutex_unlock(&m);

					//DB Access read

					if (strncmp("SELECT * FROM TABLE", threadpool[id].readbuff, 19) == 0)
					{
						for (int i = 0; i < my_buffer->rows; i++)
						{
							bzero(threadpool[id].writebuff, BUFFSIZE);
							for (int j = 0; j < my_buffer->width[i]; j++)
							{
								strcat(threadpool[id].writebuff, my_buffer->field[i][j]->text);
								strcat(threadpool[id].writebuff, "   ");
							}

							totalrecord++;
							write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
						}
					}
					else if (strncmp("SELECT DISTINCT ", threadpool[id].readbuff, 16) == 0)
					{
						char columns[BUFFSIZE];
						char columnname[64][64];

						sscanf(threadpool[id].readbuff, "SELECT DISTINCT %[^\n]s", columns);

						for (int i = 0; i < strlen(columns); i++)
						{
							if (columns[i + 1] == 'F' && columns[i + 2] == 'R' && columns[i + 3] == 'O' && columns[i + 4] == 'M')
							{
								columns[i] = '\0';
								break;
							}
						}

						char *token;
						char *rest = columns;
						int t = 0;
						while ((token = strtok_r(rest, ",", &rest)))
						{
							strcpy(columnname[t], trim(token));
							t++;
						}

						bzero(threadpool[id].writebuff, BUFFSIZE);
						for (int j = 0; j < my_buffer->width[0]; j++)
						{
							for (int k = 0; k < t; k++)
							{
								if (strcmp(my_buffer->field[0][j]->text, columnname[k]) == 0)
								{
									strcat(threadpool[id].writebuff, my_buffer->field[0][j]->text);
									strcat(threadpool[id].writebuff, "   ");
									break;
								}
							}
						}

						totalrecord++;
						write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);

						for (int i = 1; i < my_buffer->rows; i++)
						{
							char check[BUFFSIZE];
							bzero(threadpool[id].writebuff, BUFFSIZE);
							for (int j = 0; j < my_buffer->width[i]; j++)
							{
								for (int k = 0; k < t; k++)
								{
									if (strcmp(my_buffer->field[0][j]->text, columnname[k]) == 0)
									{
										strcat(threadpool[id].writebuff, my_buffer->field[i][j]->text);
										strcat(threadpool[id].writebuff, "   ");
										break;
									}
								}
							}
							int flag = 1;
							for (int o = 1; o < i; o++)
							{
								bzero(check, BUFFSIZE);
								for (int j = 0; j < my_buffer->width[o]; j++)
								{
									for (int k = 0; k < t; k++)
									{
										if (strcmp(my_buffer->field[0][j]->text, columnname[k]) == 0)
										{
											strcat(check, my_buffer->field[o][j]->text);
											strcat(check, "   ");
											if (strcmp(threadpool[id].writebuff, check) == 0)
											{
												flag = 0;
												break;
											}
										}
									}
									if (!flag)
									{
										break;
									}
								}
								if (!flag)
								{
									break;
								}
							}

							if (flag)
							{
								totalrecord++;
								write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
							}
						}
					}
					else if (strncmp("SELECT ", threadpool[id].readbuff, 7) == 0)
					{
						char columns[BUFFSIZE];
						char columnname[64][64];

						sscanf(threadpool[id].readbuff, "SELECT %[^\n]s", columns);

						for (int i = 0; i < strlen(columns); i++)
						{
							if (columns[i + 1] == 'F' && columns[i + 2] == 'R' && columns[i + 3] == 'O' && columns[i + 4] == 'M')
							{
								columns[i] = '\0';
								break;
							}
						}

						char *token;
						char *rest = columns;
						int t = 0;
						while ((token = strtok_r(rest, ",", &rest)))
						{
							strcpy(columnname[t], trim(token));
							t++;
						}

						bzero(threadpool[id].writebuff, BUFFSIZE);
						for (int j = 0; j < my_buffer->width[0]; j++)
						{
							for (int k = 0; k < t; k++)
							{
								if (strcmp(my_buffer->field[0][j]->text, columnname[k]) == 0)
								{
									strcat(threadpool[id].writebuff, my_buffer->field[0][j]->text);
									strcat(threadpool[id].writebuff, "   ");
									break;
								}
							}
						}

						totalrecord++;
						write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);

						for (int i = 1; i < my_buffer->rows; i++)
						{
							bzero(threadpool[id].writebuff, BUFFSIZE);
							for (int j = 0; j < my_buffer->width[i]; j++)
							{
								for (int k = 0; k < t; k++)
								{
									if (strcmp(my_buffer->field[0][j]->text, columnname[k]) == 0)
									{
										strcat(threadpool[id].writebuff, my_buffer->field[i][j]->text);
										strcat(threadpool[id].writebuff, "   ");
										break;
									}
								}
							}

							totalrecord++;
							write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
						}
					}
					else
					{
						bzero(threadpool[id].writebuff, BUFFSIZE);
						strcat(threadpool[id].writebuff, "This query '");
						strcat(threadpool[id].writebuff, threadpool[id].readbuff);
						strcat(threadpool[id].writebuff, "' dont implement");
						totalrecord++;
						write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
					}

					bzero(threadpool[id].writebuff, BUFFSIZE);
					strcpy(threadpool[id].writebuff, "@ENDRESULT");
					write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);

					pthread_mutex_lock(&m);
					AR--;
					if (AR == 0 && WW > 0)
					{
						pthread_cond_signal(&okToWrite);
					}
					pthread_mutex_unlock(&m);
				}
				else if (strncmp("UPDATE", threadpool[id].readbuff, 6) == 0)
				{
					pthread_mutex_lock(&m);
					while ((AW + AR) > 0)
					{
						WW++;
						pthread_cond_wait(&okToWrite, &m);
						WW--;
					}
					AW++;
					pthread_mutex_unlock(&m);

					//DB Access write

					if ((strncmp("UPDATE TABLE SET ", threadpool[id].readbuff, 17) == 0))
					{
						char columns[BUFFSIZE];

						char where[BUFFSIZE];
						char columnname[64][2][64];
						char regex[BUFFSIZE];
						char wherecolunm[64];
						char wherestate[64];

						sscanf(threadpool[id].readbuff, "UPDATE TABLE SET %[^\n]s", columns);

						for (int i = 0; i < strlen(columns); i++)
						{
							if (columns[i + 1] == 'W' && columns[i + 2] == 'H' && columns[i + 3] == 'E' && columns[i + 4] == 'R' && columns[i + 5] == 'E')
							{
								columns[i] = '\0';
								break;
							}
						}

						strcpy(regex, "UPDATE TABLE SET ");
						strcat(regex, columns);
						strcat(regex, " WHERE");
						strcat(regex, " %[^\n]s");

						sscanf(threadpool[id].readbuff, regex, where);

						char *token;
						char *token2;
						char *rest = columns;
						char *rest2;
						int t = 0;
						while ((token = strtok_r(rest, ",", &rest)))
						{
							rest2 = token;
							token2 = strtok_r(rest2, "=", &rest2);

							strcpy(columnname[t][0], trim(token2));
							token2 = strtok_r(rest2, "=", &rest2);

							if (token2[0] == '\'')
							{
								token2[0] = ' ';
							}

							if (token2[strlen(token2) - 1] == '\'')
							{
								token2[strlen(token2) - 1] = ' ';
							}

							strcpy(columnname[t][1], trim(token2));

							t++;
						}

						rest2 = where;
						token2 = strtok_r(rest2, "=", &rest2);
						strcpy(wherecolunm, trim(token2));

						strcpy(rest2, trim(rest2));
						token2 = strtok_r(rest2, "=", &rest2);
						

						if (token2[0] == '\'')
						{
							token2[0] = ' ';
						}

						if (token2[strlen(token2) - 1] == '\'')
						{
							token2[strlen(token2) - 1] = ' ';
						}
						strcpy(wherestate, trim(token2));

						for (int i = 1; i < my_buffer->rows; i++)
						{

							for (int k = 0; k < my_buffer->width[0]; k++)
							{
								if (strcmp(my_buffer->field[0][k]->text, wherecolunm) == 0)
								{
									if (strcmp(my_buffer->field[i][k]->text, wherestate) == 0)
									{
										for (int y = 0; y < t; y++)
										{
											for (int j = 0; j < my_buffer->width[i]; j++)
											{
												if (strcmp(my_buffer->field[0][j]->text, columnname[y][0]) == 0)
												{

													set_field(my_buffer->field[i][j], columnname[y][1]);
												}
											}
										}

										totalrecord++;
									}

									break;
								}
							}
						}
						char strtotalnum[10];
						bzero(threadpool[id].writebuff, BUFFSIZE);
						sprintf(strtotalnum, "%d", totalrecord);
						totalrecord++;
						strcat(threadpool[id].writebuff, "-(");
						strcat(threadpool[id].writebuff, strtotalnum);
						strcat(threadpool[id].writebuff, " rows affected)");

						write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
					}
					else
					{
						bzero(threadpool[id].writebuff, BUFFSIZE);

						strcat(threadpool[id].writebuff, "This query '");
						strcat(threadpool[id].writebuff, threadpool[id].readbuff);
						strcat(threadpool[id].writebuff, "' dont implement");

						totalrecord++;
						write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
					}

					bzero(threadpool[id].writebuff, BUFFSIZE);
					strcpy(threadpool[id].writebuff, "@ENDRESULT");
					write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);

					pthread_mutex_lock(&m);
					AW--;
					if (WW > 0)
					{
						pthread_cond_signal(&okToWrite);
					}
					else if (WR > 0)
					{
						pthread_cond_broadcast(&okToRead);
					}

					pthread_mutex_unlock(&m);
				}
				else
				{
					bzero(threadpool[id].writebuff, BUFFSIZE);

					strcat(threadpool[id].writebuff, "This query '");
					strcat(threadpool[id].writebuff, threadpool[id].readbuff);
					strcat(threadpool[id].writebuff, "' dont implement");

					totalrecord++;
					write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);

					bzero(threadpool[id].writebuff, BUFFSIZE);
					strcpy(threadpool[id].writebuff, "@ENDRESULT");
					write(threadpool[id].socketfd, threadpool[id].writebuff, BUFFSIZE);
				}
			}
			if ((strncmp("UPDATE TABLE SET ", threadpool[id].readbuff, 17) == 0))
			{
				writeLog("Thread #%d: query completed, (%d records affected)\n", id, totalrecord - 1);
			}
			else
			{
				writeLog("Thread #%d: query completed, %d records have been returned.\n", id, totalrecord - 1);
			}
		}

		close(threadpool[id].socketfd);
	}
	return NULL;
}

//for CTRL-C signal catch
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	writeLog("Termination signal received, waiting for ongoing threads to complete.\n\n");
	intsignal = 1;
	for (int i = 0; i < _larg; i++)
	{
		pthread_cond_signal(&threadpool[i].cond);
	}

	for (int i = 0; i < _larg; i++)
	{
		pthread_join(threadpool[i].queryThread_tid, NULL);
	}
	writeLog("All threads have terminated, server shutting down.\n\n");

	csv_destroy_buffer(my_buffer);

	if (sem_close(run_lock) != 0)
		writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

	sem_unlink(SERVER_NAME);

	fclose(fplogger);
	close(sockfd);
	close(sockfd);
	exit(EXIT_FAILURE);
}

void lockprogram()
{

	if ((run_lock = sem_open(SERVER_NAME, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
	{
		if (errno == EEXIST)
		{
			fprintf(stderr, "Error! the program cannot be run in more than one instance.\n");
		}
		else
		{
			fprintf(stderr, "Error! dont create Program lock semaphore because %s\n", strerror(errno));
		}
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

int main(int argc, char *argv[])
{
	lockprogram();
	signal(SIGINT, INThandler);
	int opt;

	while ((opt = getopt(argc, argv, "p:o:l:d:")) != -1)
	{
		switch (opt)
		{

		case 'p':
			_pflag = 1;
			checkArg('p', 1001, &_parg);
			break;

		case 'o':
			_oflag = 1;
			_oarg = optarg;
			break;

		case 'l':
			_lflag = 1;
			checkArg('l', 2, &_larg);
			break;

		case 'd':
			_dflag = 1;
			_darg = optarg;
			break;

		case '?':
			if (optopt == 'p' || optopt == 'o' || optopt == 'l' || optopt == 'd')
				fprintf(stderr, "Option '-%c' requires an parameter. Program terminate\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown parameter '-%c'. Program terminate\n", optopt);
			else
				fprintf(stderr, "Unknown parameter character '\\x%x'. Program terminate\n", optopt);

			if (sem_close(run_lock) != 0)
				fprintf(stderr, "Error! Close program lock semaphore : %s\n", strerror(errno));

			sem_unlink(SERVER_NAME);
			exit(EXIT_FAILURE);
			break;

		default:
			fprintf(stderr, "Unknown Error. Program terminate\n");
			if (sem_close(run_lock) != 0)
				fprintf(stderr, "Error! Close program lock semaphore : %s\n", strerror(errno));

			sem_unlink(SERVER_NAME);
			exit(EXIT_FAILURE);
			break;
		}
	}

	if (!(_pflag == 1 && _oflag == 1 && _lflag == 1 && _dflag == 1))
	{
		fprintf(stderr, "You must use all parameter. -p -o -l -d. Program terminate\n");
		if (sem_close(run_lock) != 0)
			fprintf(stderr, "Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		exit(EXIT_FAILURE);
	}

	for (; optind < argc; optind++)
	{
		fprintf(stderr, "Error extra parameter: '%s'. Program terminate\n", argv[optind]);
		if (sem_close(run_lock) != 0)
			fprintf(stderr, "Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		exit(EXIT_FAILURE);
	}

	if (_larg > SIZE)
	{
		fprintf(stderr, "Please change on code SIZE %d value because thread size %d. Recompile the code after the change. Program terminate\n", SIZE, _larg);
		if (sem_close(run_lock) != 0)
			fprintf(stderr, "Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		exit(EXIT_FAILURE);
	}

	make_daemon();

	for (int i = 0; i < _larg; i++)
	{
		threadpool[i].queryThread_id = i;
		threadpool[i].cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
		threadpool[i].isReady = 0;
		threadpool[i].socketfd = -1;
	}
	availableThread = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		writeLog("Error! Socket creation failed. %s\n", strerror(errno));

		if (sem_close(run_lock) != 0)
			writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		fclose(fplogger);
		exit(EXIT_FAILURE);
	}

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(_parg);

	if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
	{
		writeLog("Error! Socket bind failed. %s\n", strerror(errno));
		if (sem_close(run_lock) != 0)
			writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		fclose(fplogger);
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	if ((listen(sockfd, 5)) != 0)
	{
		writeLog("Error! Listen failed. %s\n", strerror(errno));
		if (sem_close(run_lock) != 0)
			writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

		sem_unlink(SERVER_NAME);
		fclose(fplogger);
		close(sockfd);
		exit(EXIT_FAILURE);
	}

	writeLog("Loading dataset...\n");

	my_buffer = csv_create_buffer();
	gettimeofday(&tv1, NULL);
	csv_load(my_buffer, _darg);
	gettimeofday(&tv2, NULL);

	timeval_subtract(&diff, &tv2, &tv1);

	writeLog("Dataset loaded in %ld.%ld seconds with %d records.\n", diff.tv_sec, diff.tv_usec, my_buffer->rows - 1);
	writeLog("A pool of %d threads has been created\n", _larg);
	int error;
	for (int i = 0; i < _larg; i++)
	{
		if ((error = pthread_create(&threadpool[i].queryThread_tid, NULL, queryThread, &threadpool[i].queryThread_id)))
		{
			writeLog("Failed to create queryThread_tid thread %d: %s\n", threadpool[i].queryThread_id, strerror(error));

			if (sem_close(run_lock) != 0)
				writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

			sem_unlink(SERVER_NAME);
			fclose(fplogger);
			close(sockfd);
			exit(EXIT_FAILURE);
		}
	}

	len = sizeof(cli);
	while (1)
	{
		pthread_mutex_lock(&lock_mutex);
		if (availableThread == 0)
		{
			writeLog("No thread is available! Waiting… \n", strerror(errno));
			pthread_cond_wait(&available_cond, &lock_mutex);
		}
		pthread_mutex_unlock(&lock_mutex);

		connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
		if (connfd < 0)
		{
			writeLog("Error! Server acccept failed. %s\n", strerror(errno));

			if (sem_close(run_lock) != 0)
				writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

			sem_unlink(SERVER_NAME);
			fclose(fplogger);
			close(sockfd);
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < _larg; i++)
		{
			pthread_mutex_lock(&lock_mutex);
			if (threadpool[i].isReady == 1)
			{
				pthread_mutex_lock(&response_mutex);

				threadpool[i].socketfd = connfd;
				pthread_cond_signal(&threadpool[i].cond);

				writeLog("A connection has been delegated to thread id #%d\n", threadpool[i].queryThread_id);

				pthread_mutex_unlock(&lock_mutex);

				pthread_cond_wait(&response_cond, &response_mutex);
				pthread_mutex_unlock(&response_mutex);
				break;
			}
			pthread_mutex_unlock(&lock_mutex);
		}
	}

	csv_destroy_buffer(my_buffer);

	if (sem_close(run_lock) != 0)
		writeLog("Error! Close program lock semaphore : %s\n", strerror(errno));

	sem_unlink(SERVER_NAME);
	fclose(fplogger);
	close(sockfd);
	return 0;
}