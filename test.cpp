#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>

int main(void)
{
	int buf ;

	struct termios ter, old ;
	int fd = STDIN_FILENO ;
	tcgetattr(fd, &old) ;
	ter = old ;
	ter.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
			| INLCR | IGNCR | ICRNL | IXON);
	ter.c_oflag &= ~OPOST;
	ter.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	ter.c_cflag &= ~(CSIZE | PARENB);
	ter.c_cflag |= CS8;
	tcsetattr(fd, TCSANOW, &ter) ;

	while (true)
	{
		buf = 0 ;
		int res = read(STDIN_FILENO, &buf, sizeof(buf)) ;
		if (res > 0)
			printf("read: %d,  %x, %c\r\n", res, (int)buf, (char)buf) ;
	}

	tcsetattr(fd, TCSANOW, &old) ;
}
