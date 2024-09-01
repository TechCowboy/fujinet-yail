#include <conio.h>
#include <ctype.h>
#include "net.h"
#include "screen_util.h"
#include "input.h"
#include "colors.h"

#define false 0
#define true 1

#define SUCCESS 1

#define JSON 1

#define OMODE_READ_WRITE 12
#define NO_TRANSLATION 0

// #define DEFAULT_SERVER "fujinet.online:5556"
#define DEFAULT_SERVER "127.0.0.1:5556"
#define DEFAULT_TOPIC "minions"

#define DEFAULT_GFX_MODE ATARI_GRAPHICS_8
#define ATARI_GRAPHICS_8 2

#define PMD85_VIDEO 0xC000
#define MAX_SIZE 8800

typedef enum _state
{
    CONNECT_SERVER,
    STREAM_IMAGES,
    CHANGE_TOPIC,
    CHANGE_SERVER,
    DONE
}
State;

typedef struct image_header
{
    unsigned char v1;
    unsigned char v2;
    unsigned char v3;
    unsigned char gfx;
    unsigned char memtkn;
    short size;
} ImageHeader;

ImageHeader header;
NetworkStatus ns;

State state = STREAM_IMAGES;

//#define BUFFER_ROWS     55
#define BUFFER_ROWS     220
#define BYTES_PER_ROW   40
byte network_buffer[BUFFER_ROWS * BYTES_PER_ROW];

#define SCR_X0    4
#define SCR_Y0    1

static char subject[40];
static char server[40];

// extern byte swap_table_00[256];
// extern byte swap_table_01[256];
// extern byte swap_table_11[256];
// extern byte swap_table_12[256];
// extern byte swap_table_22[256];
// extern byte swap_table_23[256];

// void buffer_to_screen(byte *screen)
// {
//     byte r;
//     byte i;
//     byte *b = &network_buffer[2]; // skip first 16 px
//     byte *s;
//     byte in;
//     for(r = 0; r < BUFFER_ROWS; r++)
//     {
//         s = &screen[0];
//         // process only 36 bytes from each 40 bytes line
//         for(i = 0; i < BYTES_PER_ROW-4; i += 3)
//         {
//             // input byte 0 -> output bytes 0, 1
//             in = b[0];
//             s[0] = swap_table_00[in];
//             s[1] = swap_table_01[in];
//             // input byte 1 -> output bytes 1, 2
//             in = b[1];
//             s[1] |= swap_table_11[in];
//             s[2] = swap_table_12[in];
//             // input byte 2 -> output bytes 2, 3
//             in = b[2];
//             s[2] |= swap_table_22[in];
//             s[3] = swap_table_23[in];
//             b += 3; // next input
//             s += 4; // next output
//         }
//         screen += 64;
//         b += 4; // skip last 16 px on this row and first 16 px on next row
//     }
// }


char last_key = 0;
char waitkey(byte t)
{
    last_key = 0;
    while(t)
    {
        for(byte i=0; --i;);
        if (kbhit() != 0)
        {
            last_key = cgetc();
            while(kbhit() != 0) cgetc();
            break;
        }
        t--;
    }
    return last_key;
}

char idle(unsigned char t, char wc)
{
    char kb = 0;
    char c = wc ? wc : '.';
    byte d = (c == '!') ? 10 : 50;
    textcolor(RED);
    while(t--) {
        if (!wc && t < 5) c = '1' + t;
        gotoxy(46, SCR_Y0); cputc(c); //cprintf("\x1Bp%c", c);
        if ((kb = waitkey(d)) != 0) break;
        gotoxy(46, SCR_Y0); cputc(' '); //cputs("\x1Bq ");
        if ((kb = waitkey(d)) != 0) break;
    }
    textcolor(GREEN);
    return kb;
}

void set_title(const char *s)
{
    unsigned l = strlen(s);
    screen_set_region(SCR_X0, SCR_Y0, 40, 1);
    screen_fill_region(PATTERN_BLANK);
    gotoxy(SCR_X0 + (40-l) / 2, SCR_Y0);
    textcolor(TEXT_COLOR);
    cputs(s);
}

void set_status_message(const char *s)
{
    screen_set_region(SCR_X0, SCR_Y0+29, 40, 1);
    screen_fill_region(PATTERN_BLANK);
    gotoxy(SCR_X0, SCR_Y0+29);
    textcolor(TEXT_COLOR);
    cputs(s);
}

char pop_error(const char *s, byte t)
{
    unsigned l = strlen(s);
    screen_set_region(SCR_X0, SCR_Y0+29, 40, 1);
    screen_fill_region(PATTERN_BLANK);
    gotoxy(SCR_X0, SCR_Y0+29);
    textcolor(ERROR_COLOR);
    cputs(s);
    char key = idle(t, ' ');
    screen_fill_region(PATTERN_BLANK);
    return key;
}

char* format_err(char *format, byte errno)
{
    char *s = (char *)&network_buffer[0];
    sprintf(s, format, errno);
    return s;
}

byte read_network_data(byte *dst, unsigned short len, byte tmout)
{
    unsigned short remains = len;
    unsigned short to_read;
    byte err;
    byte t = tmout * 5;

    while (remains)
    {
        // Make sure there are some data available for read
        while (!ns.bytesWaiting)
        {
            // Get # of bytes waiting to be read, via status
            err = net_status(0, &ns);
            if (err != SUCCESS)
                goto quit;

            // Wait for data if none is available
            if (!ns.bytesWaiting)
            {
                if (!tmout || t--)
                {
                    idle(1, '!');
                    continue;
                }
                else
                {
                    // Timed out
                    return 123; // TODO
                }
            }
        }

        to_read = (ns.bytesWaiting > remains) ? remains : ns.bytesWaiting;
        err = net_read(0, dst, to_read);
        if(err != SUCCESS)
            goto quit;

        dst += to_read;
        remains -= to_read;
        ns.bytesWaiting -= to_read;
    }
quit:
    return err;
}

byte load_image()
{
    byte err;

    // Image header
    err = read_network_data((byte *)&header, sizeof(header), 15);
    if (err != SUCCESS)
        goto quit;

    // Image data
    read_network_data(network_buffer, sizeof(network_buffer), 15);
    buffer_to_screen(PMD85_VIDEO + 64*8*(SCR_Y0+1)+64*4);

quit:
    return err;
}

char show_help(void)
{
    clrscr();
    set_title("YAIL for PMD 85");
    gotoxy(SCR_X0+7, SCR_Y0+2);
    cputs("Yet Another Image Loader");

    gotoxy(SCR_X0+8, SCR_Y0+5);
    textcolor(MENU_HOTKEY_COLOR); cputs("S /");
    textcolor(MENU_ITEM_COLOR); cputs(" : Change stream subject");

    gotoxy(SCR_X0+9, SCR_Y0+7);
    textcolor(MENU_HOTKEY_COLOR); cputs("K5");
    textcolor(MENU_ITEM_COLOR); cputs(" : Change server");

    gotoxy(SCR_X0+6, SCR_Y0+10);
    textcolor(TEXT_COLOR); cputs("When streaming:");

    gotoxy(SCR_X0+6, SCR_Y0+12);
    textcolor(MENU_HOTKEY_COLOR); cputs("P EOL");
    textcolor(MENU_ITEM_COLOR); cputs(" : Pause streaming");

    gotoxy(SCR_X0+2, SCR_Y0+14);
    textcolor(MENU_HOTKEY_COLOR); cputs("N SPACE >");
    textcolor(MENU_ITEM_COLOR); cputs(" : Load next image");

    return (last_key = cgetc());
}

void clear_prompt()
{
    screen_set_region(SCR_X0-1, SCR_Y0+27, 42, 3);
    screen_fill_region(PATTERN_BLANK);
}

void show_prompt(const char *p, const char *s)
{
    clear_prompt();
    gotoxy(SCR_X0, SCR_Y0+28);
    textcolor(TEXT_COLOR);
    cputs(p);
    gotoxy(SCR_X0, SCR_Y0+29);
    cputc('>');
    textcolor(EDITLINE_COLOR);
    cputs(s);
}

void change_server(void)
{
    show_prompt("Enter server", server);
    unsigned char o = strlen(server);
    input_line(SCR_X0+1, SCR_Y0+29, o, server, sizeof(server)-1, false);
    clear_prompt();
    state = CONNECT_SERVER;
}

void change_topic(void)
{
    show_prompt("Enter stream subject", subject);
    unsigned char o = strlen(subject);
    input_line(SCR_X0+1, SCR_Y0+29, o, subject, sizeof(subject)-1, false);
    clear_prompt();
    state = CONNECT_SERVER;
}

char pause_stream(void)
{
    char kb;
    set_title("PAUSED");
    while(true)
    {
        kb = idle(3, 'P');
        set_title("");
        if (kb != 0) 
        {
            if (kb == 'p' || kb == 'P' || kb == 0x0a) kb = 0;
            break;
        }
    }
    return kb;
}

void handle_keypress(byte kb)
{
    if (state == STREAM_IMAGES)
    {
        // P or EOL -> pause
        if (kb == 'p' || kb == 'P' || kb == 0x0a)
        {
            kb = pause_stream();
        }

        // keys for "next" picture
        if (kb == ' ' || kb == '>' || kb == '.' || kb == 'n' || kb == 'N' || kb == 0)
        {
            set_title(">>");
            return;
        }
    }

    bool from_help = false;
    while(true)
    {
        if (kb == '/' || kb == 's'  || kb == 'S') // [/] or [S] for topic
        {
            state = CHANGE_TOPIC;
            break;
        }
        else if (kb == KEY_K5) // [F5] for server change
        {
            state = CHANGE_SERVER;
            break;
        }
        else // any other key -> show help
        {
            if (from_help) break; // help was already shown
            kb = show_help();
            from_help = true;
        }
    }
}

void connect_server(void)
{
    byte err;
    char kb;

    clrscr();
    set_title("YAIL for PMD 85");

    // Open the connection to the server
    while(true)
    {
        set_status_message("Connecting..."); waitkey(50);
        ns.bytesWaiting = 0;
        char *s = (char *)&network_buffer[0];
        sprintf(s, "N:TCP://%s/", server);
        err = net_open(0, OMODE_READ_WRITE, NO_TRANSLATION, s);
        if (err == SUCCESS) 
        {
            set_status_message("");
            state = STREAM_IMAGES;
            break;
        }
        kb = pop_error(format_err("OPEN ERR: %u",err), 5);
        if (kb != 0)
        {
            handle_keypress(kb);
            break;
        }
    }
}

void reconnect_on_error(const char *msg)
{
    state = CONNECT_SERVER;
    char key = pop_error(msg, 5);
    if (key != 0)
    {
        handle_keypress(key);
    }
}

void stream_images(void)
{
    byte err;
    char kb;

    // Set gfx mode and search
    err = net_write(0, (byte*)"gfx 2", 5); // ATARI_GRAPHICS_8 -> 2
    if (err == SUCCESS)
    {
        char *s = (char *)&network_buffer[0];
        sprintf(s, "search \"%s\"", subject);
        err = net_write(0, (byte*)s, strlen(s));
    }
    if (err != SUCCESS)
    {
        reconnect_on_error(format_err("WRITE ERR: %u",err));
        goto quit;
    }

    //clrscr();
    set_title("YAIL for PMD 85");

    // Stream
    while(true)
    {
        err = load_image();
        if (err != SUCCESS)
        {
            reconnect_on_error(format_err("LOAD ERR: %u",err));
            goto quit;
        }

        set_title("");
        kb = idle(10, '.');
        if (kb !=0)
        {
            handle_keypress(kb);
            if (state != STREAM_IMAGES)
                goto quit;
        }

        // Request next image
        err = net_write(0, (byte*)"next", 4);
        if (err != SUCCESS)
        {
            reconnect_on_error(format_err("WRITE ERR: %u",err));
            goto quit;
        }
    }

quit:
    // Let server know
    err = net_write(0, (byte*)"quit", 4);
    // Close the connection
    net_close(0);
    ns.bytesWaiting = 0;
}

void done()
{
#asm
    jmp 0x8000
#endasm
}

int main(void)
{
    state = CONNECT_SERVER;

    strcpy(server, DEFAULT_SERVER); // default server
    strcpy(subject, DEFAULT_TOPIC); // default search

    clrscr();
    while(true)
    {
        switch(state) {
        case CONNECT_SERVER:
            connect_server();
            break;
        case STREAM_IMAGES:
            stream_images();
            break;
        case CHANGE_TOPIC:
            change_topic();
            break;
        case CHANGE_SERVER:
            change_server();
            break;
        case DONE:
            done();
            break;
        }
    }
    return 0;
}
