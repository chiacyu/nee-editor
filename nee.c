#include <stdio.h>
#include <unistd.h>
#include <termios.h>


struct termios origin_termios;


void disable_raw_mode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &origin_termios);
}


void enable_raw_mode(){
	
	tcgetattr(STDIN_FILENO, &origin_termios);
	atexit(disable_raw_mode);

	struct termios raw = origin_termios;
        raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                           | INLCR | IGNCR | ICRNL | IXON);
        raw.c_oflag &= ~OPOST;
        raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        raw.c_cflag &= ~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
   	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
	
}





int main(int argc, char *argv[]){
	
	char c;
	while(read(STDIN_FILENO, &c ,1) == 1){
		enable_raw_mode();
	}
	return 0;
}




