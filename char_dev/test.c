#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main()
{
	int ret;
	char buf[256];
	char string[256];
	int fd = open("/dev/sun_char", O_RDWR);
	
	if(fd < 0) {
		printf("error %d open sun_char...\n", fd);
		return fd;
	}

	printf("Type in a short string to send to the kernel module:\n");
	scanf("%[^\n]%*c", string);
	printf("Writing message to the device [%s].\n", string);
	ret = write(fd, string, strlen(string));
	if (ret < 0){
		perror("Failed to write the message to the device.");
		return errno;
	}
	
	printf("Press ENTER to read back from the device...\n");
	getchar();
	
	printf("Reading from the device...\n");
	ret = read(fd, buf, 256);
	if (ret < 0){
		perror("Failed to read the message from the device.");
		return errno;
	}
	printf("The received message is: [%s]\n", buf);

	close(fd);
}
