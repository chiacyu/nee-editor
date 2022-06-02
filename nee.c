#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>



#define NEE_VERSION "1.0.0"
#define CTRL_KEY(k)((k)&0x1f)
#define APBUF_INIT {NULL, 0}
#define CLEAN_SCREEN "\x1b[2J"


typedef struct{
	int screen_rows;
	int screen_cols;
	int x_index, y_index;
}device_config;

typedef struct{
	char *b;
	int len;
}append_buf;

device_config E;

struct termios origin_termios;

void prog_abort(const char *s);
void editor_draw_raws();
int get_window_size(int *rows, int *cols);
void editor_initilize();
void disable_raw_mode();
void enable_raw_mode();
char user_input_reader();
void intput_parser();
int get_cursor_position(int *rows, int *cols);
void abAppend(append_buf *ab, const char *s, int len);
void edit_init();


void edit_init(){
	E.x_index = 0;
	E.y_index = 0;

	if(get_window_size(E.screen_rows, E.screen_cols) == -1){
		prog_abort("Editor window size initialized failed!\n");
	}
}


void prog_abort(const char *s){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void abAppend(append_buf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(append_buf *ab) {
  free(ab->b);
}



void editor_draw_raws(append_buf *b){
  	int i;
  	for (i = 0; i < E.screen_rows; i++) {
		  if( i == E.screen_rows / 3){
		  	
			char welcome_msg[100];
			int welcome_msg_len = snprintf(welcome_msg , sizeof(welcome_msg), "Nee Editor -- Version %s", NEE_VERSION);

			if( welcome_msg_len > E.screen_cols){
				welcome_msg_len = E.screen_cols;
			}

			int padding = (E.screen_cols - welcome_msg_len)/2;
			if (padding){
				abAppend(b, "~", 1);
				padding--;
			}
			while (padding--){
				abAppend(b, " ", 1);
			}
		 	}else{
				abAppend(b, "~", 1);
			}
			abAppend(b, "\x1b[K]", 3);
			
			if( i < E.screen_rows - 1){
				abAppend(b, "\r\n", 2);
			}
 	 }
}


int get_window_size(int *rows, int *cols) {
  struct winsize ws;

  	if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    	
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    	return get_cursor_position(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void editor_refresh_screen(){
	append_buf ab = APBUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editor_draw_raws(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.y_index + 1, E.x_index + 1);
	abAppend(&ab, buf, strlen(buf));

	abAppend(&ab, "\x1b[H", 3);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}


int get_cursor_position(int *rows, int *cols) {

	char buf[32];
	unsigned int i;

  	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  		printf("\r\n");
  	
	while (i < sizeof(buf) - 1){
		if ( read(STDIN_FILENO, &buf[i], 1) != 1 ){
			break;
		}
		if (buf[i] == 'R'){ 
			break;
		}
		i++;
	}
  	buf[i] = '\0';

  	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  	return -1;
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
	}

	if (c == '\x1b'){
		char seq[3];


	}

}

void editor_move_cursor(char key){
	switch (key) {
		case 'a':
			E.x_index--;
			break;
		case 'd':
			E.x_index++;
			break;
		case 'w':
			E.y_index++;
			break;
		case 's':
			E.y_index--;
			break;
	}
}



void intput_parser(){
	
	char c = user_input_reader();
	
	switch(c){
		
		case(CTRL_KEY('q')):
			write(STDOUT_FILENO, "\x1b[2J", 4);
      		write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
		
		case 'w':
			editor_move_cursor(c);
			break;
		case 's':
			editor_move_cursor(c);
			break;
		case 'a':
			editor_move_cursor(c);
			break;
		case 'd':
			editor_move_cursor(c);
			break;

	}
}


int main(int argc, char *argv[]){
	
	enable_raw_mode();
	intput_parser();

  return 0;
}




