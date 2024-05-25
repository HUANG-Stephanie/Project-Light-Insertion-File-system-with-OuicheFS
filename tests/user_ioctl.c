// SPDX-License-Identifier: GPL-2.0
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define TEST_CMD _IOWR('N', 0, char *)
#define DEFRAG_CMD _IOWR('N', 1, char *)

#define SIZE 4096

int main(void)
{
	int fd, fd2, fd3;

	fd = open("/dev/ouichefs_ioctl", O_RDWR);

	if (fd < 0) {
		fprintf(stderr, "Error while opening /dev/ouichefs_ioctl\n");
		return -1;
	}

	fd2 = open("./ouichefs/ioctl_test.txt", O_RDWR | O_CREAT,
		   S_IRUSR | S_IWUSR);

	if (fd2 == -1) {
		perror("Error: not open\n");
		exit(EXIT_FAILURE);
	}

	fd3 = open("./ouichefs/test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

	if (fd3 == -1) {
		perror("Error: not open\n");
		exit(EXIT_FAILURE);
	}

	char data[14] = "Hello ioctl !\0";
	char buf[14];

	write(fd2, data, 14);
	read(fd2, buf, 14);
	printf("%s\n", buf);

	int res = ioctl(fd, TEST_CMD, fd2);

	if (res < 0) {
		perror("ioctl failed\n");
		return -1;
	}

	char data2[14] = "Hello test !\0";
	char buf2[SIZE];

	write(fd3, data2, 14);
	read(fd3, buf2, 14);
	printf("%s\n", buf2);

	memset(buf2, 0, SIZE);

	lseek(fd3, 0, SEEK_SET);
	write(fd3, "World", 5);
	lseek(fd3, 0, SEEK_SET);
	read(fd3, buf2, 19);
	printf("%s\n", buf2);

	memset(buf2, 0, SIZE);

	lseek(fd3, 4096, SEEK_SET);
	write(fd3, "Je suis à l'offset 4090", 23);
	lseek(fd3, 4096, SEEK_SET);
	read(fd3, buf2, 23);
	printf("%s\n", buf2);

	char *data3 = malloc(sizeof(char) * 4096);

	memset(data3, 'A', 4096);

	lseek(fd3, 0, SEEK_SET);
	write(fd3, data3, 4096);
	lseek(fd3, 4090, SEEK_SET);
	read(fd3, buf2, 30);
	printf("%s\n", buf2);

	memset(buf2, 0, SIZE);

	lseek(fd3, 4096 * 3, SEEK_SET);
	write(fd3, "Nouveau bloc", 12);
	lseek(fd3, 4096 * 3, SEEK_SET);
	read(fd3, buf2, 12);
	printf("%s\n", buf2);

	res = ioctl(fd, TEST_CMD, fd3);
	if (res < 0) {
		perror("ioctl failed\n");
		return -1;
	}

	res = ioctl(fd, DEFRAG_CMD, fd3);
	if (res < 0) {
		perror("ioctl failed\n");
		return -1;
	}

	res = ioctl(fd, TEST_CMD, fd3);
	if (res < 0) {
		perror("ioctl failed\n");
		return -1;
	}

	close(fd);
	close(fd2);
	close(fd3);

	return 0;
}
