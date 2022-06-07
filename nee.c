#include <sys/termios.h>
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>



#define NEE_VERSION "1.0.0"
#define TAB_STOP 8

#define CTRL_KEY(k)((k)&0x1f)
#define APBUF_INIT {NULL, 0}
#define CLEAN_SCREEN "\x1b[2J"

enum editor_keys{
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

typedef struct{
	int size;
	int rsize;
	char *chars;
	char *render;
} erows;


typedef struct{
	int screen_rows;
	int screen_cols;
	int rx;
	int x_index, y_index;
	int row_offset;
	int col_offset;
	int num_rows;
	erows *row;
	char *filename;
	char *status_msg[80];
	time_t status_msg_time;
	struct termios origin_termios;
}device_config;

typedef struct{
	char *b;
	int len;
}append_buf;

device_config E;

void prog_abort(const char *s);
void editor_draw_raws();
int get_window_size(int *rows, int *cols);
void editor_initilize();
void disable_raw_mode();
void enable_raw_mode();
int user_input_reader();
void intput_parser();
int get_cursor_position(int *rows, int *cols);
void abAppend(append_buf *ab, const char *s, int len);
void edit_init();
void editor_open();
void editor_append_row(char *s, size_t len);
void editor_update_row(erows *row);
void editor_scroll();
void editor_draw_status_bar(append_buf *b);
void editor_draw_msg_bar(append_buf *b);
int editor_row_cx_to_rx(erows *row, int cx);
void editor_set_status_message(const char *fmt, ...);
void editor_row_insert_char(erows *row, int at, int c);


void editor_scroll(){
	E.rx = 0;
	if (E.y_index < E.num_rows){
		E.rx = editor_row_cx_to_rx(&E.row[E.y_index], E.x_index);
	}

	if (E.y_index < E.row_offset){
		E.row_offset = E.y_index;
	}
	if (E.y_index >= E.row_offset + E.screen_rows){
		E.row_offset = E.y_index - E.screen_rows + 1;
	}
	if (E.rx < E.col_offset){
		E.col_offset = E.rx;
	}
	if (E.rx >= E.col_offset + E.screen_cols){
		E.col_offset = E.rx - E.screen_cols + 1;
	}
}


void editor_open(char *filename){
	free(E.filename);
	E.filename = strdup(filename);

	FILE *fp = fopen(filename, "r");
	if( !fp ){
		prog_abort("Open file error\n");
	}

	char *line =NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp) != -1){
		while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == '\r')){
			linelen--;
		}
		editor_append_row(line, linelen);
	};
	free(line);
	fclose(fp);
}


void edit_init(){

	E.x_index = 0;
	E.y_index = 0;
	E.row_offset = 0;
	E.col_offset = 0;
	E.num_rows = 0;
	E.rx = 0;
	E.row = NULL;
	E.filename = NULL;
	E.status_msg[0] = '\0';
	E.status_msg_time = 0;

	if(get_window_size(&E.screen_rows, &E.screen_cols) == -1){
		prog_abort("Editor window size initialized failed!\n");
	}
	E.screen_rows -= 2;
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


void editor_append_row(char *s, size_t len){
	E.row = realloc(E.row, sizeof(erows) * (E.num_rows + 1));
	int at = E.num_rows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editor_update_row(&E.row[at]);

	E.num_rows++;
}

void editor_update_row(erows *row){
	int tabs = 0;
	int j;
	for(j = 0 ; j<row->size ; j++){
		if(row->chars[j] == '\t'){
			tabs++;
		}
	}
	free(row->render);
	row->render = malloc(row->size + tabs*(TAB_STOP - 1) + 1);

	int idx = 0;
	for(j=0 ; j<row->size ; j++){
		if(row->chars[j]=='t'){
			row->render[idx++] = ' ';
			while (idx % TAB_STOP != 0){
				row->render[idx++] = row->chars[j];
			}
		}else{
			row->render[idx++] = row->chars[j]++;
		}
	}
	row->render[idx] = '\0';
	row->rsize = idx;
}

void editor_row_insert_char(erows *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

void editorInsertChar(int c) {
  if (E.y_index == E.num_rows) {
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.y_index], E.x_index, c);
  E.x_index++;
}

int editor_row_cx_to_rx(erows *row, int cx){
	int rx = 0;
	int j;
	for(j=0 ; j<cx ; j++){
		if(row->chars[j]=='\t'){
			rx += (TAB_STOP - -1) - (rx % TAB_STOP);
		}
		rx++;
	}
	return rx;
}


void editor_draw_raws(append_buf *b){
	int y;
	for( y=0 ; y<E.screen_rows ; y++){
		int filerow = y + E.row_offset;
		if(filerow >= E.num_rows){
			if(E.num_rows == 0 && E.screen_rows / 3){
				char welcome_msg[80];
				int welcome_len = snprintf(welcome_msg, sizeof(welcome_msg), "Nee editor -- version %s", NEE_VERSION);
				if(welcome_len > E.screen_cols) welcome_len = E.screen_cols;
				int padding = (E.screen_cols - welcome_len) / 2;
				if (padding){
					abAppend(b, "~", 1);
				}
				while (padding--) abAppend(b, " ", 1);
				abAppend(b, welcome_msg, welcome_len);
			}else{
				abAppend(b, "~", 1);
			}
		}else{
			int len = E.row[filerow].rsize - E.col_offset;
			if (len > E.screen_cols) len = 0;
			if (len > E.screen_cols) len = E.screen_cols;
			abAppend(b, &E.row[filerow].render[E.col_offset], len);
		}
		abAppend(b, "\x1b[K", 3);
		abAppend(b, "\r\n", 2);
		}

}

void editor_draw_status_bar(append_buf *b){
	abAppend(b, "\x1b[7m", 4);
	char status[80];
	char rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.num_rows);
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.y_index + 1, E.num_rows);
	if(len < E.screen_cols){
		len = E.screen_cols;
	}
	abAppend(b, status, len);
	while(len < E.screen_cols){
		if (E.screen_cols - len ==rlen){
		abAppend(b, rstatus, 1);
		break;
		} else{
			abAppend(b, " ", 1);
			len++;
		}
	}
	abAppend(b, "\x1b[m", 3);
	abAppend(b, "\r\n", 2);
}

void editor_draw_msg_bar(append_buf *b){
	abAppend(b, "\x1b[K", 3);
	int msg_len = strlen(E.status_msg);
	if(msg_len > E.screen_cols){
		msg_len = E.screen_cols;
	}
	if(msg_len && time(NULL) - E.status_msg_time < 5){
		abAppend(b, E.status_msg, msg_len);
	}
}


void editor_set_status_message(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
	va_end(ap);
	E.status_msg_time = time(NULL);
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
	editor_scroll();

	append_buf ab = APBUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editor_draw_raws(&ab);
	editor_draw_status_bar(&ab);
	editor_draw_msg_bar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.y_index - E.row_offset) + 1, (E.rx - E.col_offset) + 1);
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

int user_input_reader(){

	int nread;
	char c;

	while(1 !=  (nread = read(STDIN_FILENO, &c, 1)) ){
		if( nread == -1 && (errno != EAGAIN ) ) {
			prog_abort("Read user input failed\n");
		}
	}

	if ( c == '\x1b' ){
		char seq[3];

		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';

		if(seq[0] == '['){
			if(seq[1] >= '0' && seq[1] <= '9'){

				if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

				if(seq[2] == '~'){
					switch (seq[1]){
						case '1':
							return HOME_KEY;
							break;
						case '3':
							return DEL_KEY;
							break;
						case '4':
							return END_KEY;
							break;
						case '5':
							return PAGE_UP;
							break;
						case '6':
							return PAGE_DOWN;
							break;
						case '7':
							return HOME_KEY;
							break;
						case '8':
							return END_KEY;
							break;
					}
				}
			}else{
				switch (seq[1]){
					case 'A':
						return ARROW_UP;
						break;
					case 'B':
						return ARROW_DOWN;
						break;
					case 'C':
						return ARROW_RIGHT;
						break;
					case 'D':
						return ARROW_LEFT;
						break;
					case 'H':
						return HOME_KEY;
						break;
					case 'F':
						return END_KEY;
						break;
				}
			}
		} else{
			switch (seq[1] == 'O'){
			case 'H':
				return HOME_KEY;
				break;
			case 'F':
				return END_KEY;
				break;
			}
		}
		return '\x1b';
	}else{
		return c;
	}

}

void editor_move_cursor(int key){
	erows *row = (E.y_index >= E.num_rows) ? NULL : &E.row[E.y_index];

	switch (key) {
		case ARROW_LEFT:
			if(E.x_index){
				E.x_index--;
			}else if(E.y_index > 0){
				E.y_index--;
				E.x_index = E.row[E.y_index].size;
			}
		case ARROW_RIGHT:
			if (row && E.x_index < row->size){
			E.x_index++;
			} else if(row && E.x_index == row->size){
				E.y_index++;
				E.x_index = 0;
			}
			break;
		case ARROW_UP:
			if(E.y_index){
				E.y_index--;
			}
			break;
		case ARROW_DOWN:
			if(E.y_index < E.num_rows){
				E.y_index++;
			}
			break;
	}

	row = (E.y_index >= E.num_rows) ? NULL : &E.row[E.y_index];
	int rowlen = row ? row->size : 0;
	if (E.x_index > rowlen){
		E.x_index = rowlen;
	}
}



void intput_parser(){
	
	int c = user_input_reader();
	
	switch(c){
		
		case(CTRL_KEY('q')):
			write(STDOUT_FILENO, "\x1b[2J", 4);
      		write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case HOME_KEY:
			E.x_index = 0;
			break;
		case END_KEY:
			if (E.y_index < E.num_rows){
			E.x_index = E.row[E.y_index].size;
			}
			break;

		case PAGE_UP:
		case PAGE_DOWN:
		{
			if( c == PAGE_UP){
				E.y_index = E.row_offset;
			}
			else if ( c == PAGE_DOWN){
				E.y_index = E.row_offset + E.screen_rows - 1;
				if (E.y_index > E.num_rows){
					E.y_index = E.num_rows;
				}
			}
			int times = E.screen_rows;
			while (times--){
				editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;
		}

		case ARROW_UP:
			editor_move_cursor(c);
			break;
		case ARROW_DOWN:
			editor_move_cursor(c);
			break;
		case ARROW_LEFT:
			editor_move_cursor(c);
			break;
		case ARROW_RIGHT:
			editor_move_cursor(c);
			break;

		default:
      		editor_insert_char(c);
      		break;

	}
}


int main(int argc, char *argv[]){

	enable_raw_mode();
	edit_init();
	if (argc > 2){
		editor_open(argv[1]);
	}

	editor_set_status_message("Help : Ctrl-Q = QUIT");

	while (1){
		editor_refresh_screen();
		intput_parser();
	}

  return 0;
}




