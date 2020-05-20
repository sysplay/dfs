#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "real_sfs_ds.h"

sfs_super_block_t sb;
byte1_t block[SIMULA_FS_BLOCK_SIZE];

int init_browsing(int sfs_handle)
{
	read(sfs_handle, &sb, sizeof(sfs_super_block_t));
	/* TODO 1: Check for validity of Simula File System */
	if (sb.type != 0)
	{
		fprintf(stderr, "Invalid SFS detected. Giving up.\n");
		return -1;
	}

	printf("Welcome to SFS Browsing Shell v3.0\n\n");
	printf("Block size     : %d bytes\n", sb.block_size);
	printf("Partition size : %d blocks\n", sb.partition_size);
	printf("File entry size: %d bytes\n", sb.entry_size);
	printf("Entry tbl size : %d blocks\n", sb.entry_table_size);
	printf("Entry count    : %d\n", sb.entry_count);
	printf("\n");
	return 0;
}
void shut_browsing(int sfs_handle)
{
}

void sfs_list(int sfs_handle)
{
	int i;
	sfs_file_entry_t fe;
	time_t ts;

	/* TODO 2A: Seek to the start of file entries table, to check for its existence */
	lseek(sfs_handle, 0, SEEK_SET);
	for (i = 0; i < sb.entry_count; i++)
	{
		read(sfs_handle, &fe, sizeof(sfs_file_entry_t));
		if (!fe.name[0]) continue;
		ts = (time_t)(fe.timestamp);
		printf("%-15s  %10d bytes  %c%c%c  %s",
			fe.name, fe.size,
			fe.perms & 04 ? 'r' : '-',
			fe.perms & 02 ? 'w' : '-',
			fe.perms & 01 ? 'x' : '-',
			ctime(&ts)
			);
	}
}
void sfs_create(int sfs_handle, char *fn)
{
	int i;
	sfs_file_entry_t fe;

	/* TODO 2B: Seek to the start of file entries table, to check for its existence */
	lseek(sfs_handle, 0, SEEK_SET);
	for (i = 0; i < sb.entry_count; i++)
	{
		read(sfs_handle, &fe, sizeof(sfs_file_entry_t));
		if (!fe.name[0]) break;
		if (strcmp(fe.name, fn) == 0)
		{
			printf("File %s already exists\n", fn);
			return;
		}
	}
	if (i == sb.entry_count)
	{
		printf("No entries left\n");
		return;
	}

	lseek(sfs_handle, -(off_t)(sb.entry_size), SEEK_CUR);

	strncpy(fe.name, fn, SIMULA_FS_FILENAME_LEN);
	fe.name[SIMULA_FS_FILENAME_LEN] = 0;
	fe.size = 0;
	fe.timestamp = 0; /* TODO 3: Fill in the current timestamp */
	fe.perms = 0; /* TODO 4: Fill in the permissions as "rwx" */
	for (i = 0; i < SIMULA_FS_DATA_BLOCK_CNT; i++)
	{
		fe.blocks[i] = 0;
	}

	write(sfs_handle, &fe, sizeof(sfs_file_entry_t));
}
void sfs_remove(int sfs_handle, char *fn)
{
	printf("%s not implemented\n", __func__);
}
void sfs_update(int sfs_handle, char *fn, int *size, int update_ts, int *perms)
{
	printf("%s not implemented\n", __func__);
}
void sfs_chperm(int sfs_handle, char *fn, int perm)
{
	sfs_update(sfs_handle, fn, NULL, 0, &perm);
}
void sfs_read(int sfs_handle, char *fn)
{
	printf("%s not implemented\n", __func__);
}
void sfs_write(int sfs_handle, char *fn)
{
	printf("%s not implemented\n", __func__);
}

void usage(void)
{
	printf("Supported commands:\n");
	printf("\t?\tquit\tlist\tcreate <file>\tremove <file>\n");
	printf("\t\tchperm <0-7> <file>\tread <file>\twrite <file>\n");
}

void browse_sfs(int sfs_handle)
{
	int done;
	char cmd[256], *fn;
	int ret, perm;

	if (init_browsing(sfs_handle) != 0)
	{
		return;
	}

	done = 0;
	while (!done)
	{
		printf(" $> ");
		ret = scanf("%[^\n]", cmd);
		if (ret < 0)
		{
			done = 1;
			printf("\n");
			continue;
		}
		else
		{
			getchar();
			if (ret == 0) continue;
		}
		if (strcmp(cmd, "?") == 0)
		{
			usage();
			continue;
		}
		else if (strcmp(cmd, "quit") == 0)
		{
			done = 1;
			continue;
		}
		else if (strcmp(cmd, "list") == 0)
		{
			sfs_list(sfs_handle);
			continue;
		}
		else if (strncmp(cmd, "create", 6) == 0)
		{
			if (cmd[6] == ' ')
			{
				fn = cmd + 7;
				while (*fn == ' ') fn++;
				if (*fn != '\0')
				{
					sfs_create(sfs_handle, fn);
					continue;
				}
			}
		}
		else if (strncmp(cmd, "remove", 6) == 0)
		{
			if (cmd[6] == ' ')
			{
				fn = cmd + 7;
				while (*fn == ' ') fn++;
				if (*fn != '\0')
				{
					sfs_remove(sfs_handle, fn);
					continue;
				}
			}
		}
		else if (strncmp(cmd, "chperm", 6) == 0)
		{
			if (cmd[6] == ' ')
			{
				perm = cmd[7] - '0';
				if ((0 <= perm) && (perm <= 7) &&  (cmd[8] == ' '))
				{
					fn = cmd + 9;
					while (*fn == ' ') fn++;
					if (*fn != '\0')
					{
						sfs_chperm(sfs_handle, fn, perm);
						continue;
					}
				}
			}
		}
		else if (strncmp(cmd, "read", 4) == 0)
		{
			if (cmd[4] == ' ')
			{
				fn = cmd + 5;
				while (*fn == ' ') fn++;
				if (*fn != '\0')
				{
					sfs_read(sfs_handle, fn);
					continue;
				}
			}
		}
		else if (strncmp(cmd, "write", 5) == 0)
		{
			if (cmd[5] == ' ')
			{
				fn = cmd + 6;
				while (*fn == ' ') fn++;
				if (*fn != '\0')
				{
					sfs_write(sfs_handle, fn);
					continue;
				}
			}
		}
		printf("Unknown/Incorrect command: %s\n", cmd);
		usage();
	}

	shut_browsing(sfs_handle);
}

int main(int argc, char *argv[])
{
	char *sfs_file;
	int sfs_handle;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <partition's device file>\n", argv[0]);
		return 1;
	}
	sfs_file = argv[1];
	sfs_handle = open(sfs_file, O_RDWR);
	if (sfs_handle == -1)
	{
		fprintf(stderr, "Unable to browse SFS over %s\n", sfs_file);
		return 2;
	}
	browse_sfs(sfs_handle);
	close(sfs_handle);
	return 0;
}
