#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>


#define CTRL_KEY(k)((k)&0x1f)


struct termios origin_termios;



void prog_abort(const char *s){
	perror(s);
	exit(1);
}


void disable_raw_mode(){
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &origin_termios) == -1){
		prog_abort("Set termios attribute failed while disabling raw mode\n");
	}
	
}

void enable_raw_mode(){
	
	if(tcgetattr(STDIN_FILENO, &origin_termios) == -1){
		prog_abort("Get termios attribute failed while enabling raw mode\n");
		atexit(disable_raw_mode);
	}
	struct termios raw = origin_termios;
        raw.c_iflag &= ~(ICRNL | BRKINT | INPCK | IXON | ISTRIP);
        raw.c_oflag &= ~(OPOST);
        raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        raw.c_cflag &= ~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1){
		prog_abort("Set termios attribute failed at the end of enabling raw mode\n");
	}
}

char user_input_reader(){

	int nread;
	char c;

	while(1 !=  (nread = read(STDIN_FILENO, &c, 1)) ){
		if( nread == -1 && (errno != EAGAIN ) ) {
			prog_abort("Read user input failed\n");
		}
		return c;
	}

}


void intput_parser(){
	
	char c = user_input_reader();
	
	switch(c){
		
		case(CTRL_KEY('q')):
			break;
		
		defaut:
			printf("%d, (%c)\n");
	}
}


int main(int argc, char *argv[]){
	
	enable_raw_mode();
	intput_parser();

  return 0;
}




