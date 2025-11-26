/* File copy program. Error checking and reporting is minimal. */
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[]);
#define BUF_SIZE 4096
#define OUTPUT_MODE 0700

int main(int argc, char* argv[])
{
	int inFd, outFd, rdCount, wtCount;

	char buffer[BUF_SIZE];
	if (argc != 3)
		exit(1);
	inFd = open(argv[1], O_RDONLY);
	if (inFd < 0)
		exit(2);
	outFd = creat(argv[2], OUTPUT_MODE); /* create the destination file */
	if (outFd < 0)
		exit(3); /* Copy loop */
	while (true)
	{
		rdCount = read(inFd, buffer, BUF_SIZE); /* read a block of data */
		if (rdCount <= 0)
			break; /* if end of file or error, exit loop */
		wtCount = write(outFd, buffer, rdCount);
		if (wtCount <= 0)
			exit(4);
	} /* Close the files */
	close(inFd);
	close(outFd);
	if (rdCount == 0)
		exit(0);
	else
		exit(5);
}
