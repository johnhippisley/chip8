/*
	Written by John Hippisley (unkown date)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h> // SDL Graphics Library

SDL_Window* window;
SDL_Surface* winsurface;
SDL_Surface* buffer;

typedef unsigned char byte;
typedef unsigned short word;

const int keymap[16] =
{
    SDLK_x, SDLK_1, SDLK_2, SDLK_3,
    SDLK_q, SDLK_w, SDLK_e, SDLK_a,
    SDLK_s, SDLK_d, SDLK_z, SDLK_c,
    SDLK_4, SDLK_r, SDLK_f, SDLK_v
};

int RUN_HZ; 			// Target running speed
byte DEBUG;		 		// Whether to print debug statements
unsigned long cycles;   // Cpu cyles

byte memory[4096];	    // 4K memory
byte V[16];		 		// 16 8-bit registers
byte sp; 		 		// 8-bit stack pointer
word I, dt, st, pc; 	// Adress register, delay and sound timers, and pc
word stack[16];   	 	// 16 layer stack
byte input[16];  	 	// On/off input values
byte pixels[64*32];	 	// Monochrome screen

void initc()
{
    srand(time(0));
    pc = 0x200;
    cycles = 0;
    for(int i = 0; i < 0xF; ++i) { V[i] = 0; stack[i] = 0; input[i] = 0; }
    for(int p = 0; p < 64*32; ++p) pixels[p] = 0;
    dt = 0, st = 0, I = 0;

    SDL_Init(SDL_INIT_EVERYTHING);
    window = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 320, 0);
    winsurface = SDL_GetWindowSurface(window);
    buffer = SDL_CreateRGBSurface(0, 64, 32, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
}

void errorc()
{
    perror("Error");
    exit(-1);
}

void termc()
{
    SDL_Quit();
    printf("Quiting...\n");
    exit(0);
}

void clearscreen()
{
    for(int i = 0; i < 64*32; ++i) pixels[i] = 0;
}

void render()
{
    SDL_FillRect(winsurface, 0, 0); 	      //clear the sceen
    Uint32* pix = (Uint32 *) buffer->pixels;
    for(int i = 0; i < 64*32; ++i) pix[i] = (pixels[i] == 1 ? 0xFFFFFFFF : 0x0);
    SDL_BlitScaled(buffer, 0, winsurface, 0); //render buffer to window surface
    SDL_UpdateWindowSurface(window); 	      //render the screen
}

void beep()
{
	// Sorry... too lazy to implement
}

void evtick()
{
        SDL_Event ev;
        while(SDL_PollEvent(&ev))
        {
            if(ev.type == SDL_QUIT) termc();
            else if(ev.type == SDL_KEYDOWN)
            {
                for(int i = 0; i < 16; ++i)
                {
                    if(keymap[i] == ev.key.keysym.sym) input[i] = 1;
                }
            }
            else if(ev.type == SDL_KEYUP)
            {
                for(int i = 0; i < 16; ++i)
                {
                    if(keymap[i] == ev.key.keysym.sym) input[i] = 0;
                }
            }
        }
}

void ccheck()
{
	if(pc > 4096)
    {
    	printf("Program counter out of bounds!\n");
    	termc();
    }
    if(sp > 16)
    {
        printf("Stack pointer out of bounds\n");
        termc();
    }
    if(I > 0xFFFF)
    {
        printf("Index register out of bounds\n");
        termc();
    }
    for(int i = 0; i < 16; ++i)
    {
        if(V[i] > 255)
        {
            printf("Register %i out of bounds\n", i);
            termc();
        }
    }
}

void execute(word opcode)
{
     word nnn = opcode & 0xFFF;		 // Lowest 12 bits
     byte kk  = opcode & 0xFF; 		 // Lowest byte
     byte n   = opcode & 0xF;		 // Lowest 4 bits
     byte x   =(opcode >> 8) & 0xF;	 // Lowest 4 bytes of high byte
     byte y   =(opcode >> 4) & 0xF;  // Highest 4 bytes of the low byte

     if(DEBUG) printf("%0x => %0x ", pc, opcode);
     switch((opcode >> 12) & 0xF)
     {
         case 0x1:
             if(DEBUG) printf("(jmp)");
             pc = nnn-2; // Execute instruction 'nnn'
             break;
         case 0x2:
             if(DEBUG) printf("(jsr)");
             stack[sp] = pc;
             ++sp;
             pc = nnn - 2; // Execute instruction 'nnn'
             break;
         case 0x3:
             if(V[x] == kk) pc += 2;
             break;
         case 0x4:
             if(V[x] != kk) pc += 2;
             break;
         case 0x5:
             if(V[x] == V[y]) pc += 2;
             break;
         case 0x6:
              V[x] = kk;
              break;
         case 0x7:
             V[x] += kk;
             break;
         case 0x9:
             if(V[x] != V[y]) pc += 2;
             break;
         case 0xA:
             I = nnn;
             break;
         case 0xB:
             pc = nnn + (word) V[0] - 2; // Execute instruction 'nnn + V[0]
             break;
         case 0xC:
             V[x] = (rand() % 256) & kk;
             break;
         case 0xD:
             if(DEBUG) printf("(draw I=%i)", I);
             byte turnoff = 0;
             for(int i = 0; i < n; ++i)
             {
                 byte horizontal = memory[I + i];
                 for(int h = 0; h < 8; ++h)
                 {
                     byte dx, dy;
                     dx = V[x] + h;
                     dy = V[y] + i;
                     // Wrap x and y coords
                     if(dx >= 64) dx -= 64;
                     if(dy >= 32) dy -= 32;

                     if((dx + dy * 64) >= 0 && (dx + dy * 64) < 64 * 32)
                     {
                         byte prev = pixels[dx + dy * 64];
                         pixels[dx + dy * 64] = prev ^ ((horizontal >> (7 - h)) & 1);
                         if(pixels[dx + dy * 64] == 0 && prev == 1) turnoff = 1;
                     }
                 }
             }
             if(turnoff) V[0xF] = 1;
             else V[0xF] = 0;

             render();
             break;
         case 0x0:
             switch(n)
             {
                 case 0x0:
                     clearscreen();
                     break;
                 case 0xE:
                     if(DEBUG) printf("(ret)");
                     --sp;
                     pc = stack[sp];
                     break;
             }
         break;

         case 0x8:
             switch(n)
             {
                 case 0x0:
                     V[x] = V[y];
                     break;
                 case 0x1:
                     V[x] |= V[y];
                     break;
                 case 0x2:
                     V[x] &= V[y];
                     break;
                 case 0x3:
                     V[x] ^= V[y];
                     break;
                 case 0x4: ; // Fixes compiler error
                     word add = V[x] + V[y];
                     V[0xF] = add >> 8;
                     V[x] = add & 0xFF;
                     break;
                 case 0x5: ;
                     word sub = V[x] - V[y];
                     V[0xF] = !(V[y] > V[x] ? 1 : 0);
                     V[x] = sub & 0xFF;
                     break;
                 case 0x6:
                     V[0xF] = V[x] & 1;
                     V[x] >>= 1;
                     break;
                 case 0x7: ;
                     word suba = V[y] - V[x];
                     V[0xF] = !(V[x] > V[y] ? 1 : 0);
                     V[x] = suba & 0xFF;
                     break;
                 case 0xE:
                     V[0xF] = V[x] >> 7;
                     V[x] <<= 1;
                     break;
             }
         break;

         case 0xE:
             switch(kk)
             {
                 case 0x9E:
                     if(input[V[x]]) pc += 2;
                     break;
                 case 0xA1:
                     if(!input[V[x]]) pc += 2;
                     break;
             }
         break;

         case 0xF:
             switch(kk)
             {
                 case 0x07:
                     V[x] = dt;
                     break;
                 case 0x0A: ;
                     byte pressed = 0;
                     while(1)
                     {
                         for(int i = 0; i < 16; ++i)
                         {
                             evtick();
                             if(input[i])
                             {
                                 V[x] = i;
                                 pressed = 1;
                             }
                         }
                         if(pressed) break;
                     }
                     break;
                 case 0x15:
                     dt = V[x];
                     break;
                 case 0x18:
                     st = V[x];
                     break;
                 case 0x1E:
                     I += (word) V[x];
                     V[0xF] = I >> 12;
                     break;
                 case 0x29:
                     I = (word) V[x] * 5;
                     break;
                 case 0x33:
                     memory[I] 	 = V[x] / 100;
                     memory[I+1] = (V[x] - (memory[I] * 100)) / 10;
                     memory[I+2] = V[x] - (memory[I] * 100) - (memory[I+1] * 10);
                     break;
                 case 0x55:
                     for(int i = 0; i <= x; ++i) memory[I++ & 0xFFF] = V[i];
                     break;
                 case 0x65:
                     for(int i = 0; i <= x; ++i) V[i] = memory[I++ & 0xFFF];
                     break;
             }
        break;

        default:
            printf("Opcode %0x not found\n", opcode);
            break;
     }
     if(DEBUG) printf("\n[I=%i, mem:%i,%i,%i] [V0=%i,V1=%i,V2=%i]\n\n", (int) I, (int) memory[I], (int) memory[I+1], (int) memory[I+2],
                                                                        (int) V[0], (int) V[1], (int) V[2]);
}

int main(int argc, char** argv)
{
    // Parse command line args
    if(argc < 2)
    {
        printf("Usage: ./chip8 [filename] (-h hz) (-d)\n");
        printf("\n-h : Change target run hz\n-d : Turn on debug mode\n");
        exit(-1);
    }
    RUN_HZ = 600;
    for(int i = 0; i < argc; ++i)
    {
        if(strcmp(argv[i], "-h") == 0)
        {
            if(argc >= (DEBUG ? 5 : 4))
            {
                if(atoi(argv[i+1]) >= 60) RUN_HZ = atoi(argv[i+1]);
                else
                {
                    printf("Target HZ must be atleast 60 HZ\n");
                    exit(-1);
                }
            }
            else
            {
                printf("Usage: ./chip8 [filename] (-h hz) (-d)\n");
                printf("\n-h : Change target run hz\n-d : Turn on debug mode\n");
                exit(-1);
            }
            printf("Target HZ set to %i\n", RUN_HZ);
        }
        else if(strcmp(argv[i], "-d") == 0)
        {
            DEBUG = 1;
            printf("Debug mode turned on\n");
        }
    }

    char* path = argv[1];
    FILE* fd = fopen("res/FONT.data", "r");
    if(fd == 0) errorc();

    // Load font into memory
    int in = 0, index = 0;
    while((in = fgetc(fd)) != EOF)
    {
        memory[index] = (byte) in;
        ++index;
    }
    fclose(fd);
    fd = fopen(path, "r");

    // Load program rom into memory
    in = 0, index = 0;
    while((in = fgetc(fd)) != EOF)
    {
        memory[index + 0x200] = (byte) in;
        ++index;
    }
    fclose(fd);

    initc();
    render();
    while(1)
    {
        time_t stime = time(0);
    	// Execute one cycle
        evtick();
        word opcode = (memory[pc] << 8) | memory[pc + 1];
        execute(opcode);
        pc+=2;
        ccheck();
        if((cycles % (RUN_HZ/60)) == 0)
        {
            if(dt > 0) --dt;
            if(st > 0)
            {
                 --st;
                 beep();
            }
        }
        if(cycles >= 0xFFFFFFFF) cycles = 0;
        // Ensure we're running at proper HZ
        time_t diff = time(0) - stime;
        if((double)diff < (1000.0 / (double) RUN_HZ)) usleep((int)(((1000.0 / (double)RUN_HZ) - (double)diff) * 1000.0));
        ++cycles;
    }
    return 0;
 }
