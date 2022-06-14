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
#include <fcntl.h>
#include <sys/termios.h>

#define NEE_VERSION "1.0.0"
#define TAB_STOP 8
#define KILO_QUIT_TIMES 3

#define CTRL_KEY(k)((k)&0x1f)
#define APBUF_INIT {NULL, 0}
#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)
#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
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

enum editor_high_light{
	HL_NORMAL = 0,
	HL_COMMENT,
	HL_MLCOMMENT,
	HL_KEYWORD1,
  	HL_KEYWORD2,
	HL_STRING,
	HL_NUMBER,
	HL_MATCH
};

typedef struct{
	char *filetype;
	char **filematch;
	char **keywords;
	char *singleline_comment_start;
  	char *multiline_comment_start;
  	char *multiline_comment_end;
	int flags;
}editor_syntax;

typedef struct{
	char *b;
	int len;
}append_buf;

typedef struct{
	int idx;
	int size;
	int rsize;
	char *chars;
	char *render;
	unsigned char *hl;
	int hl_open_comment;
} erows;

typedef struct{
	int screen_rows;
	int screen_cols;
	int rx;
	int x_index, y_index;
	int row_offset;
	int col_offset;
	int num_rows;
	int dirty;
	erows *row;
	char *filename;
	char *status_msg[80];
	time_t status_msg_time;
	struct termios origin_termios;
	editor_syntax *syntax;
}device_config;

device_config E;

void prog_abort(const char *s);
void editor_draw_rows();
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
int editor_row_rx_to_cx(erows *row, int cx);
void editor_set_status_message(const char *fmt, ...);
void editor_row_insert_char(erows *row, int at, int c);
void editor_row_del_char(erows *row, int at);
char *editor_rows_to_string(int *buflen);
void editor_save();
void editor_insert_row(int at, char *s, size_t len);
void editor_del_char();
void editor_free_row(erows *row);
void editor_del_row(int at);
void editor_row_append_string(erows *row, char *s, size_t len);
void editor_insert_newline();
void editor_refresh_screen();
char *editor_prompt(char *prompt, void(*callback)(char *, int));
void editor_find();
void editor_find_call_back(char *query, int key);
void editor_update_syntax(erows *row);
void editor_select_syntax_highlight();

int is_separator(int c);

char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };

char *C_HL_keywords[] = {
  "switch", "if", "while", "for", 
  "break", "continue", "return", "else",
  "struct", "union", "typedef", "static", 
  "enum", "class", "case", "int|", 
  "long|", "double|", "float|", "char|", 
  "unsigned|", "signed|", "void|", NULL
};

editor_syntax HLDB[] = {
  {
    "c",
    C_HL_extensions,
	C_HL_keywords,
	"//", "/*", "*/",
    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS
  },
};

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

	editor_select_syntax_highlight();

	FILE *fp = fopen(filename, "r");
	if( !fp ){
		prog_abort("Open file error\n");
	}

	char *line =NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp) != -1)){
		while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen -1] == '\r')){
			linelen--;
		}
		editor_insert_row(E.num_rows, line, linelen);
	};
	free(line);
	fclose(fp);
	E.dirty = 0;
}


void edit_init(){

	E.x_index = 0;
	E.y_index = 0;
	E.row_offset = 0;
	E.col_offset = 0;
	E.num_rows = 0;
	E.rx = 0;
	E.dirty = 0;
	E.row = NULL;
	E.filename = NULL;
	E.status_msg[0] = '\0';
	E.status_msg_time = 0;
	E.syntax = NULL;

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
	E.dirty++;
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

	editor_update_syntax(row);
}

void editor_update_syntax(erows *row){
	row->hl = realloc(row->hl, row->rsize);
	memset(row->hl, HL_NORMAL, row->rsize);

	if (E.syntax == NULL) return;

	char **keywords = E.syntax->keywords;

	char *scs = E.syntax->singleline_comment_start;
	char *mcs = E.syntax->multiline_comment_start;
  	char *mce = E.syntax->multiline_comment_end;

	int scs_len = scs ? strlen(scs) : 0;
  	int mcs_len = mcs ? strlen(mcs) : 0;
  	int mce_len = mce ? strlen(mce) : 0;

	int prev_sep = 1;
	int in_string = 0;
	int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

	int i;
	while (i < row->rsize) {
		char c = row->render[i];
		unsigned char prev_hl = (i >0) ? row->hl[i - 1] : HL_NORMAL;

		if (scs_len && !in_string && !in_comment) {
      		if (!strncmp(&row->render[i], scs, scs_len)) {
        	memset(&row->hl[i], HL_COMMENT, row->rsize - i);
        	break;
      		}
    	}

		if (mcs_len && mce_len && !in_string) {
      		if (in_comment) {
        	row->hl[i] = HL_MLCOMMENT;
        if (!strncmp(&row->render[i], mce, mce_len)) {
          	memset(&row->hl[i], HL_MLCOMMENT, mce_len);
          	i += mce_len;
          	in_comment = 0;
          	prev_sep = 1;
          	continue;
        } else {
          i++;
          continue;
        }
      	} else if (!strncmp(&row->render[i], mcs, mcs_len)) {
        	memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
        	i += mcs_len;
        	in_comment = 1;
        	continue;
      	}
    	}

		if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
			if(in_string){
				row->hl[i] = HL_STRING;

				if (c == '\\' && i + 1 < row->rsize) {
          			row->hl[i + 1] = HL_STRING;
          			i += 2;
          			continue;
        		}

				if (c == in_string) in_string = 0;
				i++;
				prev_sep = 0;
				continue;
			}else{
				if( c == '"' || c == '\''){
					in_string = c;
					row->hl[i] = HL_STRING;
					i++;
					continue;
				}
			}
		}
    	if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
      		if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
          		(c == '.' && prev_hl == HL_NUMBER)) {
        	row->hl[i] = HL_NUMBER;
        	i++;
        	prev_sep = 0;
        	continue;
			}	  
      	}

		if (prev_sep) {
      		int j;
      		for (j = 0; keywords[j]; j++) {
        		int klen = strlen(keywords[j]);
        		int kw2 = keywords[j][klen - 1] == '|';
        		if (kw2) klen--;
        		if (!strncmp(&row->render[i], keywords[j], klen) &&
            		is_separator(row->render[i + klen])) {
          		memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
          		i += klen;
          		break;
				}
        	}
      
      		if (keywords[j] != NULL) {
        		prev_sep = 0;
        		continue;
      		}
    	}  
		prev_sep = is_separator(c);
		i++;
	}
	int changed = (row->hl_open_comment != in_comment);
  	row->hl_open_comment = in_comment;
  	if (changed && row->idx + 1 < E.num_rows)
    	editor_update_syntax(&E.row[row->idx + 1]);
}

int editor_syntax_to_color(int hl){
	switch (hl) {
		case HL_COMMENT:
		case HL_MLCOMMENT:
			return 36;
		case HL_KEYWORD1:
			return 33;
		case HL_KEYWORD2:
			return 32;	
		case HL_STRING:
			return 35;
		case HL_NUMBER:
			return 31;
		case HL_MATCH:
			return 34;
		default:
			return 37;
	}
}


void editor_select_syntax_highlight() {
	E.syntax = NULL;
  	if (E.filename == NULL) return;
  	char *ext = strrchr(E.filename, '.');
  	
	for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
    	editor_syntax *s = &HLDB[j];
    	unsigned int i = 0;
    while (s->filematch[i]) {
      int is_ext = (s->filematch[i][0] == '.');
      if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
          (!is_ext && strstr(E.filename, s->filematch[i]))) {
        E.syntax = s;
        int filerow;
        for (filerow = 0; filerow < E.num_rows; filerow++) {
          editor_update_syntax(&E.row[filerow]);
        }
        return;
      }
      i++;
    }
  }
}

void editor_row_insert_char(erows *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editor_update_row(row);
  E.dirty++;
}

void editor_row_del_char(erows *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editor_update_row(row);
  E.dirty++;
}

void editor_insert_newline() {
  if (E.x_index == 0) {
    editor_insert_row(E.y_index, "", 0);
  } else {
    erows *row = &E.row[E.y_index];
    editor_insert_row(E.y_index + 1, &row->chars[E.x_index], row->size - E.x_index);
    row = &E.row[E.y_index];
    row->size = E.x_index;
    row->chars[row->size] = '\0';
    editor_update_row(row);
  }
  E.y_index++;
  E.x_index = 0;
}



void editor_insert_char(int c) {
  if (E.y_index == E.num_rows) {
    editor_insert_row(E.num_rows, " ", 0);
  }
  editor_row_insert_char(&E.row[E.y_index], E.x_index, c);
  E.x_index++;
}



void editor_del_char() {
  if (E.y_index == E.num_rows) return;
  erows *row = &E.row[E.y_index];
  if (E.x_index > 0) {
    editor_row_del_char(row, E.x_index - 1);
    E.x_index--;
  }
  else{
	  E.x_index = E.row[E.y_index-1].size;
	  editor_row_append_string(&E.row[E.y_index-1], row->chars, row->size);
	  editor_del_row(E.y_index);
	  E.y_index--;
  }
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

int editor_row_rx_to_cx(erows *row, int rx){
	int cur_rx = 0;
	int cx;
	for(cx = 0 ; cx < row->size ; cx++){
		if(row->chars[cx] == '\t'){
			cur_rx += (TAB_STOP - 1) - (cur_rx % TAB_STOP);
		}
		cur_rx++;

		if(cur_rx > rx){

		}
	}

}


void editor_draw_rows(append_buf *b){
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
			char *c = &E.row[filerow].render[E.col_offset];
			unsigned char *hl = &E.row[filerow].hl[E.col_offset];
			int current_color = -1;
			int j;
			for(j = 0 ; j < len ; j++){
				if(iscntrl(c[j])){
          			char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          			abAppend(b, "\x1b[7m", 4);
          			abAppend(b, &sym, 1);
          			abAppend(b, "\x1b[m", 3);
					if (current_color != -1) {
            			char buf[16];
            			int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            			abAppend(b, buf, clen);
          			}
				} else if(hl[j] == HL_NORMAL){
					if(current_color != -1){
						abAppend(b, "\x1b[39m", 5);
						current_color = -1;
					}
					abAppend(b, &c[j], 1);
					
				}else{
					int color = editor_syntax_to_color(hl[j]);
					if(color != current_color){
						current_color = color;
						char buf[16];
						int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
						abAppend(b, buf, clen);
					}
					abAppend(b, &c[j], 1);
				}
			}
			abAppend(b, "\x1b[39m", 5);
		}
		abAppend(b, "\x1b[K", 3);
		abAppend(b, "\r\n", 2);
		}

}

void editor_draw_status_bar(append_buf *b){
	abAppend(b, "\x1b[7m", 4);
	char status[80];
	char rstatus[80];

	int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.num_rows, E.dirty ? "(modified)" : "");
	int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no ft", E.y_index + 1, E.num_rows);

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

char *editor_rows_to_string(int *buflen){
	int totlen = 0;
	int j;
	for (j = 0; j < E.num_rows; j++){
		totlen += E.row[j].size + 1;
	}
  	*buflen = totlen;
  	char *buf = malloc(totlen);
  	char *p = buf;
  	for (j = 0; j < E.num_rows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editor_row_append_string(erows *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editor_update_row(row);
  E.dirty++;
}

void editor_free_row(erows *row){
	free(row->render);
	free(row->chars);
	free(row->hl);
}

void editor_insert_row(int at, char *s, size_t len) {
  if (at < 0 || at > E.num_rows) return;

  E.row = realloc(E.row, sizeof(erows) * (E.num_rows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erows) * (E.num_rows - at));
  for (int j = at + 1; j <= E.num_rows; j++) E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editor_update_row(&E.row[at]);
  E.num_rows++;
  E.dirty++;
}

void editor_del_row(int at) {
  if (at < 0 || at >= E.num_rows) return;
  editor_free_row(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erows) * (E.num_rows - at - 1));
  for (int j = at; j < E.num_rows - 1; j++) E.row[j].idx--;
  E.num_rows--;
  E.dirty++;
}

void editor_save() {
  if (E.filename == NULL){
	  E.filename = editor_prompt("Save as: %s", NULL);
	  if (E.filename == NULL){
		  editor_set_status_message("Save aborted");
		  return;
	  }
	  editor_select_syntax_highlight();
  } 
  int len;
  char *buf = editor_rows_to_string(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
		E.dirty = 0;
		editor_set_status_message("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editor_set_status_message("Fail to perform I/O task", strerror(errno));
}


char *editor_prompt(char *prompt, void(*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';
  while (1) {
    editor_set_status_message(prompt, buf);
    editor_refresh_screen();

    int c = user_input_reader();
	if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
    if (buflen != 0) buf[--buflen] = '\0';
    }else if (c == '\x1b') {
        editor_set_status_message("");
		if (callback){
			callback(buf, c);
		}
		free(buf);
        return NULL;
    } else if (c == '\r'){
		if(buflen != 0){
			editor_set_status_message("");
			if (callback) callback(buf, c);
			return buf;
		}
	} 
	else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
	if (callback) callback(buf, c);
  }
}

int is_separator(int c){
	return isspace(c) || c == '\0' ||  strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editor_find_call_back(char *query, int key){
	static int last_match = -1;
	static int direction = 1;

	static int saved_hl_line;
	static char *saved_hl = NULL;

	if(saved_hl){
		memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
		free(saved_hl);
		saved_hl = NULL;
	}


	if (key == '\r' || key == '\x1b'){
		last_match = -1;
		direction = 1;
		return;
	} else if(key == ARROW_RIGHT || key == ARROW_DOWN){
		direction = 1;
	} else if(key == ARROW_LEFT || key == ARROW_UP){
		direction = -1;
	} else{
		last_match = -1;
		direction = 1;
	}

	if(last_match == -1){
		direction = 1;
	}

	int current = last_match;
	int i;
	for(i = 0 ; i < E.num_rows ; i++){
		current += direction;
		if(current == -1){
			current = E.num_rows - 1;
		}else if(current == E.num_rows){
			current = 0;
		}
		erows *row = &E.row[i];
		char *match = strstr(row->render, query);
		if (match){
			last_match = current;
			E.y_index = current;
			E.x_index = editor_row_rx_to_cx(row, row->render);
			E.row_offset = E.num_rows;

			saved_hl_line = current;
			saved_hl = malloc(row->rsize);
			memcpy(saved_hl, row->hl, row->rsize);
			memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
			break;
		}
	}
	free(query);
}

void editor_find(){
	int saved_x_index = E.x_index;
	int saved_y_index = E.y_index;
	int saved_col_off = E.col_offset;
	int saved_row_off = E.row_offset;

	char *query = editor_prompt( "Search: %s (Use ESC/Arrows/Enter)", editor_find_call_back);

	if(query){
		free(query);
	} else{
		E.x_index = saved_x_index;
		E.y_index = saved_y_index;
		E.col_offset = saved_col_off;
		E.row_offset = saved_row_off;
	}	
}


int get_window_size(int *rows, int *cols){
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

	editor_draw_rows(&ab);
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
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.origin_termios) == -1){
		prog_abort("Set termios attribute failed while disabling raw mode\n");
	}
	
}

void enable_raw_mode(){
	
	if(tcgetattr(STDIN_FILENO, &E.origin_termios) == -1){
		prog_abort("Get termios attribute failed while enabling raw mode\n");
		atexit(disable_raw_mode);
	}
	struct termios raw = E.origin_termios;
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
	static int quit_times = KILO_QUIT_TIMES;
	
	int c = user_input_reader();
	
	switch(c){
		
		case '\r':
		editor_insert_newline();
			break;

		
		case(CTRL_KEY('q')):
			if (E.dirty && quit_times > 0) {
        		editor_set_status_message("WARNING!!! File has unsaved changes. "
          		"Press Ctrl-Q %d more times to quit.", quit_times);
        	quit_times--;
        	return;
      		}
			break;


    	case CTRL_KEY('s'):
      		editor_save();
      		break;
		

		case HOME_KEY:
			E.x_index = 0;
			break;
		case END_KEY:
			if (E.y_index < E.num_rows){
			E.x_index = E.row[E.y_index].size;
			}
			break;

		case CTRL_KEY('f'):
			editor_find();
			break;

		case BACKSPACE:
    	case CTRL_KEY('h'):
    	case DEL_KEY:
			if (c == DEL_KEY) editor_move_cursor(ARROW_RIGHT);
      		editor_del_char();
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
    	case CTRL_KEY('l'):
    	case '\x1b':
      		break;

		default:
      		editor_insert_char(c);
      		break;

	}
	quit_times = KILO_QUIT_TIMES;
}


int main(int argc, char *argv[]){

	enable_raw_mode();
	edit_init();
	if (argc > 2){
		editor_open(argv[1]);
	}

	editor_set_status_message("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

	while (1){
		editor_refresh_screen();
		intput_parser();
	}

  return 0;
}




