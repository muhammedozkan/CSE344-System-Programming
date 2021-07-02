/*---------------------------------------------//
//Gebze Technical University                   //
//CSE344 Systems Programming Course            //
//Muhammed OZKAN 151044084                     //
//---------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

//for getopt parameter
int _wflag = 0, _fflag = 0, _bflag = 0, _tflag = 0, _pflag = 0, _lflag = 0;
char *warg = NULL, *farg = NULL, *barg = NULL, *targ = NULL, *parg = NULL, *larg = NULL;

//for nicely printed tree
int screencontrol = 0;
char currentpath[PATH_MAX];
int sinyal=0;

//for CTRL-C signal catch
void INThandler(int sig)
{
	signal(sig, SIG_IGN);
	sinyal=0;
}

//to calculate printing path level
int treeLevel(char *path1, char *path2)
{
	int level = 0;
	for (int i = 0; i < strlen(path1); i++)
	{
		if (strlen(path2) > i)
		{
			if (path1[i] == path2[i])
			{
				if (path1[i] == '/' && path2[i] == '/')
					level++;
			}
			else
			{
				return --level;
			}
		}
		else
			return level;
	}

	return level;
}

//to decide according to search criteria
int isValid(const char *directory_name, const char *name)
{
	int result = 0;

	char *fullpath = (char*) malloc(2 + strlen(directory_name) + strlen(name));
	strcpy(fullpath, directory_name);
	strcat(fullpath, "/");
	strcat(fullpath, name);

	struct stat fileStat;
	lstat(fullpath, &fileStat);

	char permissions[9];
	permissions[0] = (fileStat.st_mode &S_IRUSR) ? 'r' : '-';
	permissions[1] = (fileStat.st_mode &S_IWUSR) ? 'w' : '-';
	permissions[2] = (fileStat.st_mode &S_IXUSR) ? 'x' : '-';
	permissions[3] = (fileStat.st_mode &S_IRGRP) ? 'r' : '-';
	permissions[4] = (fileStat.st_mode &S_IWGRP) ? 'w' : '-';
	permissions[5] = (fileStat.st_mode &S_IXGRP) ? 'x' : '-';
	permissions[6] = (fileStat.st_mode &S_IROTH) ? 'r' : '-';
	permissions[7] = (fileStat.st_mode &S_IWOTH) ? 'w' : '-';
	permissions[8] = (fileStat.st_mode &S_IXOTH) ? 'x' : '-';

	char type;
	switch (fileStat.st_mode &S_IFMT)
	{
		case S_IFBLK:
			type = 'b';
			break;
		case S_IFCHR:
			type = 'c';
			break;
		case S_IFDIR:
			type = 'd';
			break;
		case S_IFIFO:
			type = 'p';
			break;
		case S_IFLNK:
			type = 'l';
			break;
		case S_IFREG:
			type = 'f';
			break;
		case S_IFSOCK:
			type = 's';
			break;
		default:
			type = 'u';	//unknown
			break;
	}

	free(fullpath);

	if (_fflag)
	{
		char *filename = (char*) malloc(1 + strlen(name));
		strcpy(filename, name);

		for (int i = 0; i <= strlen(filename); i++)
		{
			if (filename[i] >= 65 && filename[i] <= 90)
				filename[i] = filename[i] + 32;
		}

		char *pch = strstr(farg, "+");

		if (pch != NULL)
		{
			for (int i = 0; i < strlen(filename); i++)
			{
				for (int j = 0; j < strlen(farg); j++)
				{
					if (filename[i] == farg[j])
					{
						i++;
					}
					else if (farg[j] == '+' && i > 0 && j > 0)
					{
						if (filename[i] == farg[j - 1])
						{
							i++;
							j--;
						}
						else if (filename[i] == farg[j + 1])
						{
							i++;
							j++;
						}
						else
						{
							free(filename);
							return 0;
						}
					}
					else
					{
						free(filename);
						return 0;
					}
				}
			}

			free(filename);
			result = 1;
			
		}
		else
		{
			if (!strcmp(filename, farg))
			{
				free(filename);
				result = 1;
			}
			else
			{
				free(filename);
				return 0;
			}
		}
	}

	if (_bflag)
	{
		if (fileStat.st_size == atoi(barg))
			result = 1;
		else
			return 0;
	}

	if (_tflag)
	{
		if (type == targ[0])
			result = 1;
		else
			return 0;
	}

	if (_pflag)
	{
		char *perm = (char*) malloc(10);
		strcpy(perm, permissions);

		if (!strcmp(perm, parg))
		{
			free(perm);
			result = 1;
		}
		else
		{
			free(perm);
			return 0;
		}
	}

	if (_lflag)
	{
		if (fileStat.st_nlink == atoi(larg))
			result = 1;
		else
			return 0;
	}

	return result;
}

int listDirectory(const char *directory_name)
{	
	int count = 0;
	if(sinyal)
	{
		DIR * directory;
	

	//Open the directory
	directory = opendir(directory_name);

	//Check was opened 
	if (!directory)
	{
		fprintf(stderr, "Cannot open directory '%s': %s. \t Program continue...\n", directory_name, strerror(errno));
	}
	else
	{
		while (1)
		{
			struct dirent * content;
			const char *name;

			//Read directory
			content = readdir(directory);
			if (!content)
			{
				//no more entries directory break
				break;
			}

			name = content->d_name;

			//Print the name of the file or directory if it matches the search criteria
			if (isValid(directory_name, name))
			{
				char givenpath[PATH_MAX];
				realpath(warg, givenpath);

				char filepath[PATH_MAX];
				int path_length;
				
				path_length = snprintf(filepath, PATH_MAX, "%s", directory_name);
				
				if (path_length >= PATH_MAX)
					{
						fprintf(stderr, "The path length is too long. Program terminate\n");
						exit(EXIT_FAILURE);
					}

				char actualfilepath[PATH_MAX];
				realpath(filepath, actualfilepath);

				if (screencontrol == 0)
				{
					printf("%s\n", givenpath);
					screencontrol = 1;
				}

				char temp[PATH_MAX];
				strcpy(temp, &actualfilepath[strlen(givenpath)]);

				const char s[2] = "/";
				char *token;
				int i = 1;
				token = strtok(temp, s);

				int tmp = treeLevel(&actualfilepath[strlen(givenpath)], currentpath);

				while (token != NULL)
				{
					if (tmp == 0)
					{
						printf("|");
						for (int j = 0; j < i; j++)
							printf("--");

						printf("%s\n", token);
					}
					else
						tmp--;

					token = strtok(NULL, s);
					i++;
				}

				printf("|");

				for (int j = 0; j < i; j++)
					printf("--");
				printf("%s\n", name);

				strcpy(currentpath, &actualfilepath[strlen(givenpath)]);
				count++;
			}

			//check directory
			if (content->d_type &DT_DIR)
			{
				if (strcmp(name, "..") != 0 &&
					strcmp(name, ".") != 0)
				{
					int path_length;
					char path[PATH_MAX];

					path_length = snprintf(path, PATH_MAX, "%s/%s", directory_name, name);

					if (path_length >= PATH_MAX)
					{
						fprintf(stderr, "The path length is too long. Program terminate\n");
						exit(EXIT_FAILURE);
					}

					//Recursively call 
					count += listDirectory(path);
				}
			}
		}

		//close the directory
		if (closedir(directory))
		{
			fprintf(stderr, "Directory could not be closed '%s': %s. Program terminate\n", directory_name, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}}
	return count;
}

int main(int argc, char *argv[])
{
	signal(SIGINT, INThandler);
	sinyal=1;
	int opt;

	while ((opt = getopt(argc, argv, "w:f:b:t:p:l:")) != -1)
	{
		switch (opt)
		{
			case 'w':
				_wflag = 1;
				warg = optarg;

				break;

			case 'f':
				_fflag = 1;

				if (optarg[0] != '+')
					farg = optarg;
				else
				{
					fprintf(stderr, "The -f parameter cannot begin with the '+' sign. Program terminate\n");
					exit(EXIT_FAILURE);
				}

				for (int i = 0; i <= strlen(farg); i++)
				{
					if (farg[i] >= 65 && farg[i] <= 90)
						farg[i] = farg[i] + 32;
					if (i > 0 && i < strlen(farg))
					{
						if (farg[i - 1] == '+' && farg[i] == '+')
						{
							fprintf(stderr, "The -f parameter cannot have a + sign twice in a row. Program terminate\n");
							exit(EXIT_FAILURE);
						}
					}
				}

				break;

			case 'b':
				_bflag = 1;

				if ((optarg[0] == '0' || optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '3' || optarg[0] == '4' || optarg[0] == '5' || optarg[0] == '6' || optarg[0] == '7' || optarg[0] == '8' || optarg[0] == '9'))
					barg = optarg;
				else
				{
					fprintf(stderr, "The -b parameter must be an integer specifying the file size in bytes. Program terminate\n");
					exit(EXIT_FAILURE);
				}

				break;

			case 't':
				_tflag = 1;

				if (strlen(optarg) == 1 && (optarg[0] == 'd' || optarg[0] == 's' || optarg[0] == 'b' || optarg[0] == 'c' || optarg[0] == 'f' || optarg[0] == 'p' || optarg[0] == 'l'))
					targ = optarg;
				else
				{
					fprintf(stderr, "The -t parameter can only use them as a single character d: directory, s: socket, b: block device, c: character device f: regular file, p: pipe accepts l: symbolic link. Program terminate\n");
					exit(EXIT_FAILURE);
				}

				break;

			case 'p':
				_pflag = 1;

				if (strlen(optarg) == 9)
					parg = optarg;
				else
				{
					fprintf(stderr, "The -p parameter can be 9 characters long. Program terminate\n");
					exit(EXIT_FAILURE);
				}

				for (int i = 0; i <= strlen(parg); i++)
				{
					if (parg[i] >= 65 && parg[i] <= 90)
						parg[i] = parg[i] + 32;
				}

				for (int i = 0; i < 9; i++)
				{
					if (!(parg[i] == 'r' || parg[i] == 'w' || parg[i] == 'x' || parg[i] == '-'))
					{
						fprintf(stderr, "The -p parameter can only contain r w x -. Program terminate\n");
						exit(EXIT_FAILURE);
					}
				}

				break;

			case 'l':
				_lflag = 1;

				if ((optarg[0] == '0' || optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '3' || optarg[0] == '4' || optarg[0] == '5' || optarg[0] == '6' || optarg[0] == '7' || optarg[0] == '8' || optarg[0] == '9'))
					larg = optarg;
				else
				{
					fprintf(stderr, "The -l parameter must be an integer specifying the number of links. Program terminate\n");
					exit(EXIT_FAILURE);
				}

				break;

			case '?':
				if (optopt == 'w' || optopt == 'f' || optopt == 'b' || optopt == 't' || optopt == 'p' || optopt == 'l')
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

	if (!_wflag)
	{
		fprintf(stderr, "The '-w' parameter is required. Program terminate\n");
		exit(EXIT_FAILURE);
	}

	if (!(_fflag == 1 || _bflag == 1 || _tflag == 1 || _pflag == 1 || _lflag == 1))
	{
		fprintf(stderr, "You must use at least one parameter. -f -b -t -p -l. Program terminate\n");
		exit(EXIT_FAILURE);
	}

	for (; optind < argc; optind++)
	{
		fprintf(stderr, "Error extra parameter: '%s'. Program terminate\n", argv[optind]);
		exit(EXIT_FAILURE);
	}

	//call search function
	if (!listDirectory(warg))
	{
		printf("No file found.\n");
	}
	
	if(!sinyal)
		printf("\nProgram terminated because CTRL-C keys were pressed.\n");
	
	return 0;
}