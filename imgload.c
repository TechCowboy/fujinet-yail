#include <conio.h>
#include <atari.h>
#include <peekpoke.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// Some defines to make my life easier
#define SAVMSC 0x58  // Stores address for start of screen memory
#define RAMTOP 0x6A 
#define MEMTOP 0x2E5
#define SDLSTL 0x230
#define DLIST 0xD402

// Important ASCII codes
#define ASC_TAB 0x09
#define ASC_LF 0x0A
#define ASC_CR 0x15
#define ASC_SPC 0x20
#define ASC_HASH 0x23
#define ASC_0 0x30

// This organizes the display list before the screen memory
#define MY_DL 0x6C00
#define MY_SCRN_MEM (MY_DL + 0x0400) // 1024 byte aligned
#define MY_SCRN_MEM_B 0x8000
#define MY_SCRN_MEM_C 0x9000
#define FILENAME "MTFUJI.PGM"

typedef unsigned char byte;  // just a little easier to use

#define READ_BYTE(FD, B) \
    if(!read(FD, &B, 1)) \
        return -1;

// A simple structure for defining a display list in a code compact way
struct dl_def
{
    byte blank_lines;
    byte mode;    // From the Antic modes
    byte lines;   // # of lines of the mode
    unsigned address; // Address of screen memory for mode, 0x0000 if use SAVMSC + offset
};

// Bit masks for setPixel
byte masks[] = { 128, 64, 32, 16, 8, 4, 2, 1, 0 };

// Globals
int fd;
int count;
byte b, buff[256];
int w = 0;
int h = 0;
int mxp = 16; // max pixel value
int val;

//
unsigned readLine()
{
    count = 0;
    while(1)
    {
        if(!read(fd, &b, 1))
            return count;

        if(b == ASC_LF)
            return count;

        buff[count++] = b;
    }
}

// Simple power of ten functin
unsigned pow10(unsigned val, byte exp)
{
    unsigned v = val;
    int i;
    for(i = 0; i < exp; i++)
        v *= 10;

    return v;
}

// Convert buff into integer
void parseWidthHeight()
{
    int* v = &h;
    int i, dc;
    dc = 0;
    for(i = count-1; i >=0; i--)
    {
        if(buff[i] == ASC_SPC)
        {
            v = &w;
            dc = 0;
            //printf("\n");
        }
        else
        {
            unsigned digit = (unsigned)(buff[i] - ASC_0);
            *v += pow10(digit, (unsigned)dc);
            //printf("%d, %d: %02x %d %d\n", i, dc, buff[i], digit, *v);
            dc++;           
        }
    }
}

//
unsigned readComment()
{
    while(1)
    {
        READ_BYTE(fd, b)
        if(b == ASC_LF)
        {
            printf("\n");
            break;
        }
        printf("%02x ", (char)b);
    }

    return 0;
}

// Builds a display list that is defined with an array of dl_def's, placing it at dl_location.
// dl_location - location in memory for the DL
// dl_def[]    - an array of dl_defs that are used to define the modes
// n           - the number of dl_def entries
unsigned makeDisplayList(void* dl_location, struct dl_def dl[], unsigned n)
{
    byte* dl_mem = dl_location;
    int idx;
    for(idx = 0; idx < n; idx++)
    {
        int jdx;
        struct dl_def* entry = &dl[idx];

        // Handle blanks, should calculate how many... just using base 8 for now
        for(jdx = 0; jdx < entry->blank_lines/8; jdx++)
            *(dl_mem++) = (byte)DL_BLK8;

        // Now handle mode.  No memory, just mode line, else DL_LMS
        for(jdx = 0; jdx < entry->lines; jdx++)
        {
            if(jdx)
                *(dl_mem++) = (byte)entry->mode;
            else
            {
                if(entry->address)
                {
                    *(dl_mem++) = DL_LMS(entry->mode);
                    *(((unsigned*)dl_mem)++) = entry->address;
                }
                else
                    *(dl_mem++) = (byte)entry->mode;
            }
        }
    }

    // Finish up and add jump line
    *(dl_mem++) = DL_JVB;
    *(((unsigned*)dl_mem)++) = (unsigned)dl_location;

    return (unsigned)(dl_mem - (byte*)dl_location);
}

// Shows the contents of a display list.
// name - simply used in the output header so you can tell which DL is which on the console.
// mloc - the location of the DL in memory 
void print_dlist(const char* name, void* mloc)
{
    byte b = 0x00, low, high;
    unsigned idx = 0;

    printf("Displaylist %s at (%p)\n", name, mloc);
    while(1)
    {
        if(idx)
            printf(", ");

        // Get the instruction
        b = ((byte*)mloc)[idx];
        if((b & 0x40) && (b & 0x0F)) // these have two address bytes following)
        {
            low = ((byte*)mloc)[++idx];
            high = ((byte*)mloc)[++idx];
            printf("%02X (%02x%02x)", b, low, high);
        }
        else
            printf("%02X", b);

        idx++;

        if(b == 0x41) // JVB so done... maybe  have to add code to look at the address
            break;
    }
}

// Set a pixel to a value
// *NOTE* only really works with Gr8 right now which has pixels in bits of bytes.
// mem - screen buffer location
// w   = screen buffer width (pixels)
// h   - screen buffer height
// x   - pixel x location
// y   - pixel y location
// v   - value to set
void setPixel(byte* mem,
              unsigned w, unsigned h, unsigned x, unsigned y,
              byte v)
{
    byte* row = &(mem[y * w/8]);
    byte* b = row + x / 8;
    byte p = (byte)(x % (unsigned)8);

    byte a = *b ^ v;
    a &= masks[p];
    *b ^= a;
}

//
void readIntoGfx9(int fd, void* dmem, unsigned numbytes)
{
    const unsigned MAX_SIZE = 80 * 34; // Max size of temp buffer (2720 bytes) about 2/3rds
    void* tmem = (void*)(MY_SCRN_MEM_C + 0x0400);
    unsigned numread, i;
    while(numbytes)
    {
        numread = numbytes > MAX_SIZE ? MAX_SIZE : numbytes;
        read(fd, (void*)tmem, numread);
        
        // Process
        for(i = 0; i < numread/2; i++)
            ((byte*)dmem)[i] |= (( ((byte*)tmem)[i*2] & 0xF0) | (( ((byte*)tmem)[i*2+1] & 0xF0) >> 4));

        (byte*)dmem = (byte*)dmem + numread/2;

        numbytes -= numread;
    }
}


//
int main()
{
    const unsigned memw = 320;
    //const unsigned memh = 192;
    const unsigned memh = 220;
    int i, j;
    // We are creating a header of Gr8 to the standard 24 lines of text.
    struct dl_def dls[] = { {8, DL_MAP320x1x1, 102, MY_SCRN_MEM},
                            {0, DL_MAP320x1x1, 102, MY_SCRN_MEM_B},
                            {0, DL_MAP320x1x1, 16, MY_SCRN_MEM_C} };

    // Generate a display list and show the values.
    makeDisplayList((void*)MY_DL, dls, 3);

    //print_dlist("Main", MY_DL);
    //printf("Hit <Return>\n");
    //cgetc();

    fd = open(FILENAME, O_RDONLY);
    if(fd >= 0)
    {
        count = 0;

        // Read the header
        count = readLine();

        if(count != 2)
            return -2;

        count = 0;

        // Check for comment
        count = readLine();

        if(buff[0] == ASC_HASH)  // Just read a comment, read more
            count = readLine();

        // Read the digits
        printf("%d: ", count);
        for(i = 0; i < count; i++)
            printf("%02x ", buff[i]);
        printf("\n");
        parseWidthHeight();

        #ifndef GR_8
        count = readLine();
        mxp = 0; //buff[0];
        printf("## %d: ", count);
        for(i = 0; i < count; i++)
            printf("%02x ", buff[i]);
        printf("\n");

        // Parse it
        {
            int i, dc = 0;
            for(i = count-1; i >=0; i--)
            {
                unsigned digit = (unsigned)(buff[i] - ASC_0);
                mxp += pow10(digit, (unsigned)dc);
                //printf("%d, %d: %02x %d %d\n", i, dc, buff[i], digit, *v);
                dc++;
            } 
        }

        #endif

        printf("** %u, %u, %u\n", w, h, (unsigned)mxp);

        printf("Hit <Return>\n");
        cgetc();

        // Read the data into the framebuffer
        memset((void*)MY_SCRN_MEM, 0x00, 0x1000);//(memw/8)*memh);
        memset((void*)MY_SCRN_MEM_B, 0x00, 0x1000);
        memset((void*)MY_SCRN_MEM_C, 0x00, 0x1000);

        {
        int w_8 = (int)w/8;
        if(w % 8)
            w_8++;   // round up

        #ifdef GR_8
        read(fd, (void*)MY_SCRN_MEM, w_8*102);
        read(fd, (void*)MY_SCRN_MEM_B, w_8*102);
        read(fd, (void*)MY_SCRN_MEM_C, w_8*16);
        #else
        /* Super basic but works
        for(j=0; j<102; j++)
        {
            byte pxs[2];
            for(i=0; i < 40; i++)
            {
                read(fd, (void*)pxs, 2); // read two bytes at a time.  We will stuff into the display memory
                pxs[0] /= (byte)16;
                pxs[1] /= (byte)16;
                ((byte*)MY_SCRN_MEM)[j * 40 + i] |= ((pxs[0] << 4) | pxs[1]);
            }
        }
        for(j=0; j<102; j++)
        {
            byte pxs[2];
            for(i=0; i < 40; i++)
            {
                read(fd, (void*)pxs, 2); // read two bytes at a time.  We will stuff into the display memory
                pxs[0] /= (byte)16;
                pxs[1] /= (byte)16;
                ((byte*)MY_SCRN_MEM_B)[j * 40 + i] |= ((pxs[0] << 4) | pxs[1]);
            }
        }
        for(j=0; j<16; j++)
        {
            byte pxs[2];
            for(i=0; i < 40; i++)
            {
                read(fd, (void*)pxs, 2); // read two bytes at a time.  We will stuff into the display memory
                pxs[0] /= (byte)16;
                pxs[1] /= (byte)16;
                ((byte*)MY_SCRN_MEM_C)[j * 40 + i] |= ((pxs[0] << 4) | pxs[1]);
            }
        }
        */
        /* maybe faster
        // Use two-thirds of MEM_C as a temporary read buffer to speed things up
        // Read 3K ish blocks...
        {
        byte* tbuff = (byte*)((void*)(MY_SCRN_MEM_C + 0x0400));
        byte* sbuff = (byte*)MY_SCRN_MEM;
        for(j = 0; j < 3; j++)
        {
            read(fd, (void*)tbuff, 80 * 34); // 1/3
            // Process
            for(i = 0; i < 40*34; i++)
                sbuff[i] |= ((tbuff[i*2] & 0xF0) | ((tbuff[i*2+1] & 0xF0) >> 4));
            sbuff = sbuff + (40 * 34);
        }

        sbuff = (byte*)MY_SCRN_MEM_B;
        for(j = 0; j < 3; j++)
        {
            read(fd, (void*)tbuff, 80 * 34); // 1/3
            // Process
            for(i = 0; i < 40*34; i++)
                sbuff[i] |= ((tbuff[i*2] & 0xF0) | ((tbuff[i*2+1] & 0xF0) >> 4));
            sbuff = sbuff + (40 * 34);
        }

        sbuff = (byte*)MY_SCRN_MEM_C;
        read(fd, (void*)tbuff, 80 * 16); // ... last bit
        // Process
        for(i = 0; i < 40*16; i++)
            sbuff[i] |= ((tbuff[i*2] & 0xF0) | ((tbuff[i*2+1] & 0xF0) >> 4));
        }
        */
        readIntoGfx9(fd, (void*)MY_SCRN_MEM, 80 * 102);
        readIntoGfx9(fd, (void*)MY_SCRN_MEM_B, 80 * 102);
        readIntoGfx9(fd, (void*)MY_SCRN_MEM_C, 80 * 16);
        #endif

        printf("Hit <Return> %d x %d = %d\n", w_8, h, w_8 * h);
        cgetc();

        #ifdef GR_8
        //POKE(709, 0);
        //POKE(710, 15);
        #else
        POKE(623,64);
        #endif
        POKEW(SDLSTL, MY_DL);
        }

        /*count = readLine();
        printf("$$$$  %0d\n", count);
        {
        int w_8 = (int)w/8;
        if(w % 8)
            w_8++;   // round up
    printf("Hit <Return> %d x %d = %d\n", w_8, h, w_8 * h);
    cgetc();
        for(i = 0; i < h; i++)
        {
            for(j = 0; j < w_8; j++)
            {
                //READ_BYTE(fd, b)
                //printf("%02x ", b);
                //if(b > 0x00)
                //    setPixel((void*)MY_SCRN_MEM, memw, memh, j, i, b);
                ((byte*)MY_SCRN_MEM)[i * memw/8 + j] |= ~buff[i * w_8 + j];
            }
        }
        }
        */
    }
    else
    {
        printf("Unable to open the file %s\n", FILENAME);
    }
    

    //printf("Hit <Return> to quit\n");
    cgetc();

    return 0;
}