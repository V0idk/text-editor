#include <termios.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <list>
#include <fstream>
#include <sys/ioctl.h>
#include <string.h>
#include <deque>
#include <sstream>

using namespace std;

static struct termios orig_termios;

deque<string> lines;
char *filename;
int wx;
int x = 1,y = 1;
int screenrows, screencols;

int enableRawMode() {
    struct termios raw;

    if (!isatty(STDIN_FILENO)) goto fatal;
    if (tcgetattr(STDIN_FILENO,&orig_termios) == -1) goto fatal;

    auto disableRawMode = [](){
        tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
    };

    atexit(disableRawMode);

    raw = orig_termios;
    raw.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) < 0) goto fatal;
    return 0;

    fatal:
    errno = ENOTTY;
    return -1;
}

enum KEY_ACTION{
    CTRL_H = 8,         /* \b 向前删除*/
    ENTER = 13,         /* Enter */
    CTRL_Q = 17,        /* Ctrl-q */
    CTRL_S = 19,        /* Ctrl-s */
    ESC = 27,           /* Escape */
    BACKSPACE =  127,   /* Backspace */
    //软解码
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY, /* delete是向后删除. 此处实现统一向前*/
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

//捕捉对文件造成影响的按键并标识。这里针对字符序列软解码
int editorReadKey(int fd) {
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
            case ESC:
                if (read(fd,seq,1) == 0) return ESC;
                if (read(fd,seq+1,1) == 0) return ESC;
                if (seq[0] == '[') {
                    if (seq[1] >= '0' && seq[1] <= '9') {
                        if (read(fd,seq+2,1) == 0) return ESC;
                        if (seq[2] == '~') {
                            switch(seq[1]) {
                                case '3': return DEL_KEY;
                                case '5': return PAGE_UP;
                                case '6': return PAGE_DOWN;
                            }
                        }
                    } else {
                        switch(seq[1]) {
                            case 'A': return ARROW_UP;
                            case 'B': return ARROW_DOWN;
                            case 'C': return ARROW_RIGHT;
                            case 'D': return ARROW_LEFT;
                            case 'H': return HOME_KEY;
                            case 'F': return END_KEY;
                        }
                    }
                }
                break;
            default:
                return c;
        }
    }
}

void editorSave(){
    char tmpname[256];
    ofstream f;
    sprintf (tmpname, "tmpfileXXXXXX");
    int fd = mkstemp(tmpname);
    ofstream output(tmpname);
    int bytes = 0;
    for(auto line:lines){
        output << line << endl;
        bytes += line.size() + 1;
    }
    remove(filename);
    rename(tmpname,filename);

    char buf[50];
    snprintf(buf,sizeof(buf),"\x1b[%d;0H\x1b[0K\x1b[7m%d%s\x1b[0m",screenrows,bytes," bytes written on disk");
    write(STDOUT_FILENO,buf,strlen(buf));
}

void arrowDown(){
    if(wx+x >= lines.size())
        return;
    if(x < screenrows - 1){
        if(y > lines[wx + x ].size())
            y = lines[wx + x ].size() +1;
        x += 1;
    }else if(x == screenrows - 1){
        if(wx < lines.size())
            wx += 1;
    }
}

void arrowUp(){
    if(x == 1) {
        if(wx == 0)
            return;
        if(y > lines[wx + x - 2].size())
            y = lines[wx + x - 2].size() + 1;
        wx -= 1;
    }
    else if(x > 1) {
        if(y > lines[wx + x - 2].size())
            y = lines[wx + x - 2].size() + 1;
        x -= 1;
    }
}

void delChar(){
    if( y == 1){
        if(x == 1)
            return;
        lines[wx + x - 2] += lines[wx + x - 1];
        lines.erase(lines.begin() + wx + x - 1);
        y = lines[wx + x - 2].size() + 1;
        x -= 1;
    }else if( y > 1){
        lines[wx + x - 1].erase(lines[wx + x - 1].begin()+ y - 2 );
        y -= 1;
    }
}

void editorProcessKeypress(int fd) {
    int c = editorReadKey(fd);
    //current row: lines[wx + x - 1]
    switch(c) {
        case ENTER:
            if(y <= lines[wx + x -1].size()){
                lines.insert(lines.begin() + wx + x ,
                             string(lines[wx + x - 1].begin()+ y - 1, lines[wx + x - 1].end() ));
                lines[wx + x - 1].erase(lines[wx + x - 1].begin()+ y - 1 ,lines[wx + x - 1].end());
            }else{
                lines.insert(lines.begin() + wx + x ,string());
            }
            arrowDown();
            y = 1;
            break;
        case CTRL_Q:
            exit(0);
            break;
        case CTRL_S:
            editorSave();
            break;
        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            delChar();
            break;
        case PAGE_UP:
            if(wx < screenrows - 1)
                wx = 0;
            else
                wx -= screenrows - 1;
            x = y = 1;
            break;
        case PAGE_DOWN:
            if(wx + screenrows - 1 >= lines.size())
                wx = lines.size() - screenrows + 1;
            else
                wx += screenrows - 1;
            x = y = 1;
            break;
        case ARROW_UP:
            arrowUp();
            break;
        case ARROW_DOWN:
            arrowDown();
            break;
        case ARROW_LEFT:
            if(y!=0) y -= 1;
            break;
        case ARROW_RIGHT:
            if (y <= lines[wx + x -1].size())
                y += 1;
            break;
        case ESC:
            break;
        default:
            lines[wx + x -1].insert(lines[wx + x -1].begin() + y - 1,c);
            y += 1;
            break;
    }
}

void readFile(){
    ifstream input(filename);
    string line;
    while (getline(input, line, '\n')){
        if( !line.empty() && (line[line.size()-1] == '\r' || line[line.size()-1] == '\n'))
            line.pop_back();
        lines.push_back(line);
    }
}

int getCursorPosition() {
    char buf[32];
    unsigned int i = 0;

    /* Report cursor location */
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    /* Read the response: ESC [ rows ; cols R */
    while (i < sizeof(buf)-1) {
        if (read(STDIN_FILENO,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* Parse it. */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",screenrows,screencols) != 2) return -1;
    return 0;
}

int getWindowSize() {
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() failed. Try to query the terminal itself. */
        int orig_row, orig_col, retval;

        /* Get the initial position so we can restore it later. */
        retval = getCursorPosition();
        if (retval == -1) goto failed;

        /* Go to right/bottom margin and get position. */
        if (write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        retval = getCursorPosition();
        if (retval == -1) goto failed;

        /* Restore position. */
        char seq[32];
        snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
        if (write(STDOUT_FILENO,seq,strlen(seq)) == -1) {
            /* Can't recover... */
        }
        return 0;
    } else {
        screencols = ws.ws_col;
        screenrows = ws.ws_row;
        return 0;
    }

    failed:
    return -1;
}

void textWindowFresh(){
    stringstream buffer;
    buffer << "\x1b[0;0H";
    int tabnum = 0;
    for(int i=0;i < screenrows -1 ;i++){
        if(wx+i >= lines.size() ){
            buffer << "\x1b[0K" << "~";
            buffer << "\r\n";
        }
        else{
            string line;
            int t = lines[wx+i].size();
            for(auto c : lines[wx+i]){
                if(c == '\t'){
                    line += "  ";
                    tabnum++;
                }
                else
                    line += c;
            }
            buffer << "\x1b[0K" << line;
            buffer << "\r\n";
        }
    }
    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",x,y+tabnum);
    buffer << buf;
    write(STDOUT_FILENO,buffer.str().c_str(),buffer.str().size());
}

void initEditor(){
    string buf;
    buf += "~\r\n";
    for(int i=0;i<screenrows -1 ;i++)
        buf += "~\r\n";
    write(STDOUT_FILENO,buf.c_str(),buf.size());
}

int main(int argc, char **argv){
    filename = argv[1];
    enableRawMode();
    initEditor();
    readFile();
    getWindowSize();
    while(1) {
        textWindowFresh();
        editorProcessKeypress(STDIN_FILENO);
    }
}