/*
 * MasterMind implementation: template; see comments below on which parts need to be completed
 * CW spec: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2022.pdf
 * This repo: https://gitlab-student.macs.hw.ac.uk/f28hs-2021-22/f28hs-2021-22-staff/f28hs-2021-22-cwk2-sys

 * Compile:
 gcc -c -o lcdBinary.o lcdBinary.c
 gcc -c -o master-mind.o master-mind.c
 gcc -o master-mind master-mind.o lcdBinary.o
 * Run:
 sudo ./master-mind

 OR use the Makefile to build
 > make all
 and run
 > make run
 and test
 > make test

 ***********************************************************************
 * The Low-level interface to LED, button, and LCD is based on:
 * wiringPi libraries by
 * Copyright (c) 2012-2013 Gordon Henderson.
 ***********************************************************************
 * See:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
*/

/* ======================================================= */
/* SECTION: includes                                       */
/* ------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#include <unistd.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

/* --------------------------------------------------------------------------- */
/* Config settings */
/* you can use CPP flags to e.g. print extra debugging messages */
/* or switch between different versions of the code e.g. digitalWrite() in Assembler */
#define DEBUG
#undef ASM_CODE

// =======================================================
// Tunables
// PINs (based on BCM numbering)
// For wiring see CW spec: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2022.pdf
// GPIO pin for green LED
#define LED 13
// GPIO pin for red LED
#define LED2 5
// GPIO pin for button
#define BUTTON 19
// =======================================================
// delay for loop iterations (mainly), in ms
// in mili-seconds: 0.2s
#define DELAY 200
// in micro-seconds: 3s
#define TIMEOUT 3000000
// =======================================================
// APP constants   ---------------------------------
// number of colours and length of the sequence
#define COLS 3
#define SEQL 3
// =======================================================

// generic constants

#ifndef TRUE
#define TRUE (1 == 1)
#define FALSE (1 == 2)
#endif

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

// =======================================================
// Wiring (see inlined initialisation routine)

#define STRB_PIN 24
#define RS_PIN 25
#define DATA0_PIN 23
#define DATA1_PIN 10
#define DATA2_PIN 27
#define DATA3_PIN 22

/* ======================================================= */
/* SECTION: constants and prototypes                       */
/* ------------------------------------------------------- */

// =======================================================
// char data for the CGRAM, i.e. defining new characters for the display

static unsigned char newChar[8] =
    {
        0b11111,
        0b10001,
        0b10001,
        0b10101,
        0b11111,
        0b10001,
        0b10001,
        0b11111,
};

/* Constants */

static const int colors = COLS;
static const int seqlen = SEQL;

static char *color_names[] = {"red", "green", "blue"};

static int *theSeq = NULL;

static int *seq1, *seq2, *cpy1, *cpy2;

/* --------------------------------------------------------------------------- */

// data structure holding data on the representation of the LCD
struct lcdDataStruct
{
  int bits, rows, cols;
  int rsPin, strbPin;
  int dataPins[8];
  int cx, cy;
};

static int lcdControl;

/* ***************************************************************************** */
/* INLINED fcts from wiringPi/devLib/lcd.c: */
// HD44780U Commands (see Fig 11, p28 of the Hitachi HD44780U datasheet)

#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY 0x04
#define LCD_CTRL 0x08
#define LCD_CDSHIFT 0x10
#define LCD_FUNC 0x20
#define LCD_CGRAM 0x40
#define LCD_DGRAM 0x80

// Bits in the entry register

#define LCD_ENTRY_SH 0x01
#define LCD_ENTRY_ID 0x02

// Bits in the control register

#define LCD_BLINK_CTRL 0x01
#define LCD_CURSOR_CTRL 0x02
#define LCD_DISPLAY_CTRL 0x04

// Bits in the function register

#define LCD_FUNC_F 0x04
#define LCD_FUNC_N 0x08
#define LCD_FUNC_DL 0x10

#define LCD_CDSHIFT_RL 0x04

// Mask for the bottom 64 pins which belong to the Raspberry Pi
//	The others are available for the other devices

#define PI_GPIO_MASK (0xFFFFFFC0)

static unsigned int gpiobase;
static uint32_t *gpio;

static int timed_out = 0;
struct lcdDataStruct *lcd;
/* ------------------------------------------------------- */
// misc prototypes

int failure(int fatal, const char *message, ...);
void waitForEnter(void);
void waitForButton(uint32_t *gpio, int button);

// /* ======================================================= */
// /* SECTION: hardware interface (LED, button, LCD display)  */
// /* ------------------------------------------------------- */
// /* low-level interface to the hardware */

// /* ********************************************************** */
// /* COMPLETE the code for all of the functions in this SECTION */
// /* Either put them in a separate file, lcdBinary.c, and use   */
// /* inline Assembler there, or use a standalone Assembler file */
// /* You can also directly implement them here (inline Asm).    */
// /* ********************************************************** */
// /* These are just prototypes; you need to complete the code for each function */
// In lcdBinary.c

// /* wait for a button input on pin number @button@; @gpio@ is the mmaped GPIO base address */
// /* can use readButton(), depending on your implementation */
// void waitForButton(uint32_t *gpio, int button);

/* ======================================================= */
/* SECTION: game logic                                     */
/* ------------------------------------------------------- */
/* AUX fcts of the game logic */

/* ********************************************************** */
/* COMPLETE the code for all of the functions in this SECTION */
/* Implement these as C functions in this file                */
/* ********************************************************** */

/* initialise the secret sequence; by default it should be a random sequence */

void initSeq()
{
  // Allocating memory for the array
  theSeq = (int *)malloc(seqlen * sizeof(int));

  // Checking the memory allocation
  if (theSeq == NULL)
  {
    printf("Memory allocation failed.\n");
    exit(1);
  }

  // Initializing the random number generator
  srand((unsigned int)time(NULL));

  // Filling the array with random values between 1 and seqlen
  int i = 0;
  while (i < seqlen)
  {
    theSeq[i] = (rand() % seqlen) + 1;
    i++;
  }
}

/* display the sequence on the terminal window, using the format from the sample run in the spec */
void showSeq(int *seq)
{
  // Iterating through seq, converting numbers to corresponding R G B code and printing it
  int i = 0;
  while (i < seqlen)
  {
    if (seq[i] == 1)
    {
      fprintf(stderr, "%s", " R");
    }
    else if (seq[i] == 2)
    {
      fprintf(stderr, "%s", " G");
    }
    else if (seq[i] == 3)
    {
      fprintf(stderr, "%s", " B");
    }
    else
    {
      fprintf(stderr, "%s", "  ");
    }
    i++;
  }
  fprintf(stderr, " \n");
}

#define NAN1 8
#define NAN2 9

/* counts how many entries in seq2 match entries in seq1 */
/* returns exact and approximate matches, either both encoded in one value, */
/* or as a pointer to a pair of values */

//Implementation in C and Inline Assembly
int *countMatches(int *seq1, int *seq2)
{
  // Initializing the counters for exact and approx matches
  int exact = 0;
  int approx = 0;

  // Allocating memory for an array to mark visited elements
  int *visited = (int *)malloc(seqlen * sizeof(int));

  // Iterating through array and setting each element to 0 to mark as not visited
  int i = 0;
  asm volatile(
      ".initialize_loop:\n"
      // Storing value to be initialized in val(ie val=0)
      "mov %[val], #0\n"                             
      // Making visited[i] = 0 
      "str %[val], [%[visited], %[index], lsl #2]\n" 
      // Increamenting i
      "add %[index], %[index], #1\n"             
      // Comparing i with seqlen    
      "cmp %[index], %[seqlen]\n"                 
      // Jumping back to initialize_loop if i < seqlen   
      "blt .initialize_loop\n"                       
      //Output
      : [visited] "+r"(visited), [index] "+r"(i), [val] "=r"(i)
      : [seqlen] "r"(seqlen)
      // Clobbered registers
      : "cc");

  
  // Initializing the index variable i
  i = 0;
  
  // Iterating through both sequences to find exact matches
  asm volatile(
      ".exact_match_loop:\n"
      // Loading seq1[i] into r4
      "ldr r4, [%[seq1], %[index], lsl #2]\n" 
      // Loading seq2[i] into r5
      "ldr r5, [%[seq2], %[index], lsl #2]\n" 
      // Comparing seq1[i] and seq2[i]
      "cmp r4, r5\n"                     
      // If not equal, incrementing i     
      "bne .increment_i\n"                    
      // Incrementing exact (r2)
      "add %[exact], %[exact], #1\n"          
      "mov r6, #1\n"
      // Marking visited[i] = 1
      "str r6, [%[visited], %[index], lsl #2]\n" 
      ".increment_i:\n"
      // Increamenting i
      "add %[index], %[index], #1\n" 
      // Comparing i with seqlen (r8)
      "cmp %[index], %[seqlen]\n"    
      // Jumping to exact_match_loop if i < seqlen
      "blt .exact_match_loop\n"  
      // Output    
      : [exact] "+r"(exact)
      : [seq1] "r"(seq1), [seq2] "r"(seq2), [visited] "r"(visited), [index] "r"(i), [seqlen] "r"(seqlen)
      // Clobbered registers
      : "r4", "r5", "r6", "cc");

  // Initializing the index variables i and j
  i = 0;
  int j = 0;

  // Iterating through both sequences to find approximate matches
  asm volatile(
      ".approx_match_loop:\n"
      // Loading seq1[i] into r4
      "ldr r4, [%[seq1], %[i], lsl #2]\n" 
      // Loading seq2[i] into r5
      "ldr r5, [%[seq2], %[i], lsl #2]\n" 
      // Comparing seq1[i] and seq2[i]
      "cmp r4, r5\n"                      
      // If equal, incrementing i
      "beq .increment_i_approx\n"         
      // Initializing j to 0
      "mov %[j], #0\n" 
      ".approx_inner_loop:\n"
      // Loading seq1[j] into r6
      "ldr r6, [%[seq1], %[j], lsl #2]\n"    
      // Loading visited[j] into r7
      "ldr r7, [%[visited], %[j], lsl #2]\n" 
      // Comparing seq2[i] and seq1[j]
      "cmp r5, r6\n"                         
      // If not equal, incrementing j
      "bne .increment_j_approx\n"        
      // Comparing visited[j] with 0    
      "cmp r7, #0\n"                         
      // If not equal, incrementing j
      "bne .increment_j_approx\n"            
      // Incrementing approx
      "add %[approx], %[approx], #1\n" 
      "mov r8, #1\n"
      // Marking visited[j] = 1
      "str r8, [%[visited], %[j], lsl #2]\n" 
      // Breaking inner loop, incrementing i
      "b .increment_i_approx\n"              

      ".increment_j_approx:\n"
      // Incrementing j
      "add %[j], %[j], #1\n"     
      // Comparing j with seqlen
      "cmp %[j], %[seqlen]\n"    
      // Jumping back to approx_inner_loop if j < seqlen
      "blt .approx_inner_loop\n" 

      ".increment_i_approx:\n"
      // Increamenting i
      "add %[i], %[i], #1\n"     
      // Comparing i with seqlen
      "cmp %[i], %[seqlen]\n"    
      // Jump to approx_match_loop if i < seqlen
      "blt .approx_match_loop\n" 
      // Output
      : [approx] "+r"(approx), [i] "+r"(i), [j] "+r"(j)
      : [seq1] "r"(seq1), [seq2] "r"(seq2), [visited] "r"(visited), [seqlen] "r"(seqlen)
      // Clobbered registers
      : "r4", "r5", "r6", "r7", "r8", "cc");

  // Allocateing memory for the output array
  int *output = malloc(2 * sizeof(int));
  // Storing the matches in array
  output[0] = exact;
  output[1] = approx;

  free(visited); // Freeing memory allocated for the visited array

  return output; // Returning output
}

// Implementation in C only
// int *countMatches(int *seq1, int *seq2)
// {
//   // Initializing the counters for exact and approx matches
//   int exact = 0;
//   int approx = 0;

//   // Allocating memory for an array to mark visited elements
//   int *visited = (int *)malloc(seqlen * sizeof(int));

//   // Iterating through array and setting each element to 0 to mark as not visited
//   for (int i = 0; i < seqlen; i++)
//   {
//     visited[i] = 0;
//   }

//   // Iterating through both sequences to find exact matches
//   for (int i = 0; i < seqlen; i++)
//   {
//     // Finding exact matches
//     if (seq1[i] == seq2[i])
//     {
//       // Incrementing the exact and marking that position as visited
//       exact++;
//       visited[i] = 1;
//     }
//   }

//   // Iterating through both sequences to find approximate matches
//   for (int i = 0; i < seqlen; i++)
//   {
//     // Checking if the elements at the same position are not equal
//     if (seq1[i] != seq2[i])
//     {
//       // Iterating through seq1 to find a matching element
//       for (int j = 0; j < seqlen; j++)
//       {
//         // Checking if an element in seq1 matches the current element in seq2 and has not been visited
//         if (seq2[i] == seq1[j] && !visited[j])
//         {
//           // Incrementing the approx matches and marking the element in seq1 as visited
//           approx++;
//           visited[j] = 1;
//           break;
//         }
//       }
//     }
//   }

//   // Allocateing memory for the output array
//   int *output = malloc(2 * sizeof(int));
//   // Storing the matches in array
//   output[0] = exact;
//   output[1] = approx;

//   free(visited); // Freeing memory allocated for the visited array

//   return output; // Returning output
// }

/* show the results from calling countMatches on seq1 and seq1 */
void showMatches(int *code, /* only for debugging */ int *seq1, int *seq2, /* optional, to control layout */ int lcd_format)
{
  // Printing Exact and Approx Matches
  printf("Exact : %d\n", code[0]);
  printf("Approx : %d\n", code[1]);
}

/* parse an integer value as a list of digits, and put them into @seq@ */
/* needed for processing command-line with options -s or -u            */
void readSeq(int *seq, int val)
{

  // Variable to calculate  number of digits in val
  int num_digits = 0;
  int temp = val;
  while (temp != 0)
  {
    // Incrementing digit count
    num_digits++;
    // Shifting number to the right by one digit
    temp /= 10;
  }
  if (num_digits == 0)
  {
    num_digits = 1;
  }

  // Add integer digits to the array (seq) passed as an argument
  for (int i = num_digits - 1; i >= 0; --i)
  {
    seq[i] = val % 10;
    val /= 10;
  }
}

/* read a guess sequence fron stdin and store the values in arr */
/* only needed for testing the game logic, without button input */
int readNum(int max)
{
  /* ***  COMPLETE the code here  ***  */
  // Not Used!!
}

/* ======================================================= */
/* SECTION: TIMER code                                     */
/* ------------------------------------------------------- */
/* TIMER code */

/* timestamps needed to implement a time-out mechanism */
static uint64_t startT, stopT;

/* ********************************************************** */
/* COMPLETE the code for all of the functions in this SECTION */
/* Implement these as C functions in this file                */
/* ********************************************************** */

/* you may need this function in timer_handler() below  */
/* use the libc fct gettimeofday() to implement it      */
uint64_t timeInMicroseconds()
{
  /* ***  COMPLETE the code here  ***  */
  // Not Used!!
}

/* this should be the callback, triggered via an interval timer, */
/* that is set-up through a call to sigaction() in the main fct. */
void timer_handler(int signum)
{
  /* ***  COMPLETE the code here  ***  */
  // Not Used!!
}

/* initialise time-stamps, setup an interval timer, and install the timer_handler callback */
void initITimer(uint64_t timeout)
{
  /* ***  COMPLETE the code here  ***  */
  // Not Used!!
}

/* ======================================================= */
/* SECTION: Aux function                                   */
/* ------------------------------------------------------- */
/* misc aux functions */

int failure(int fatal, const char *message, ...)
{
  va_list argp;
  char buffer[1024];

  if (!fatal) //  && wiringPiReturnCodes)
    return -1;

  va_start(argp, message);
  vsnprintf(buffer, 1023, message, argp);
  va_end(argp);

  fprintf(stderr, "%s", buffer);
  exit(EXIT_FAILURE);

  return 0;
}

/*
 * waitForEnter:
 *********************************************************************************
 */

void waitForEnter(void)
{
  printf("Press ENTER to continue: ");
  (void)fgetc(stdin);
}

/*
 * delay:
 *	Wait for some number of milliseconds
 *********************************************************************************
 */

void delay(unsigned int howLong)
{
  struct timespec sleeper, dummy;

  sleeper.tv_sec = (time_t)(howLong / 1000);
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000;

  nanosleep(&sleeper, &dummy);
}

/* From wiringPi code; comment by Gordon Henderson
 * delayMicroseconds:
 *	This is somewhat intersting. It seems that on the Pi, a single call
 *	to nanosleep takes some 80 to 130 microseconds anyway, so while
 *	obeying the standards (may take longer), it's not always what we
 *	want!
 *
 *	So what I'll do now is if the delay is less than 100uS we'll do it
 *	in a hard loop, watching a built-in counter on the ARM chip. This is
 *	somewhat sub-optimal in that it uses 100% CPU, something not an issue
 *	in a microcontroller, but under a multi-tasking, multi-user OS, it's
 *	wastefull, however we've no real choice )-:
 *
 *      Plan B: It seems all might not be well with that plan, so changing it
 *      to use gettimeofday () and poll on that instead...
 *********************************************************************************
 */

void delayMicroseconds(unsigned int howLong)
{
  struct timespec sleeper;
  unsigned int uSecs = howLong % 1000000;
  unsigned int wSecs = howLong / 1000000;

  /**/ if (howLong == 0)
    return;
#if 0
  else if (howLong  < 100)
    delayMicrosecondsHard (howLong) ;
#endif
  else
  {
    sleeper.tv_sec = wSecs;
    sleeper.tv_nsec = (long)(uSecs * 1000L);
    nanosleep(&sleeper, NULL);
  }
}

/* ======================================================= */
/* SECTION: LCD functions                                  */
/* ------------------------------------------------------- */
/* medium-level interface functions (all in C) */

/* from wiringPi:
 * strobe:
 *	Toggle the strobe (Really the "E") pin to the device.
 *	According to the docs, data is latched on the falling edge.
 *********************************************************************************
 */

void strobe(const struct lcdDataStruct *lcd)
{

  // Note timing changes for new version of delayMicroseconds ()
  digitalWrite(gpio, lcd->strbPin, 1);
  delayMicroseconds(50);
  digitalWrite(gpio, lcd->strbPin, 0);
  delayMicroseconds(50);
}

/*
 * sentDataCmd:
 *	Send an data or command byte to the display.
 *********************************************************************************
 */

void sendDataCmd(const struct lcdDataStruct *lcd, unsigned char data)
{
  register unsigned char myData = data;
  unsigned char i, d4;

  if (lcd->bits == 4)
  {
    d4 = (myData >> 4) & 0x0F;
    for (i = 0; i < 4; ++i)
    {
      digitalWrite(gpio, lcd->dataPins[i], (d4 & 1));
      d4 >>= 1;
    }
    strobe(lcd);

    d4 = myData & 0x0F;
    for (i = 0; i < 4; ++i)
    {
      digitalWrite(gpio, lcd->dataPins[i], (d4 & 1));
      d4 >>= 1;
    }
  }
  else
  {
    for (i = 0; i < 8; ++i)
    {
      digitalWrite(gpio, lcd->dataPins[i], (myData & 1));
      myData >>= 1;
    }
  }
  strobe(lcd);
}

/*
 * lcdPutCommand:
 *	Send a command byte to the display
 *********************************************************************************
 */

void lcdPutCommand(const struct lcdDataStruct *lcd, unsigned char command)
{
#ifdef DEBUG
  fprintf(stderr, "lcdPutCommand: digitalWrite(%d,%d) and sendDataCmd(%d,%d)\n", lcd->rsPin, 0, lcd, command);
#endif
  digitalWrite(gpio, lcd->rsPin, 0);
  sendDataCmd(lcd, command);
  delay(2);
}

void lcdPut4Command(const struct lcdDataStruct *lcd, unsigned char command)
{
  register unsigned char myCommand = command;
  register unsigned char i;

  digitalWrite(gpio, lcd->rsPin, 0);

  for (i = 0; i < 4; ++i)
  {
    digitalWrite(gpio, lcd->dataPins[i], (myCommand & 1));
    myCommand >>= 1;
  }
  strobe(lcd);
}

/*
 * lcdHome: lcdClear:
 *	Home the cursor or clear the screen.
 *********************************************************************************
 */

void lcdHome(struct lcdDataStruct *lcd)
{
#ifdef DEBUG
  fprintf(stderr, "lcdHome: lcdPutCommand(%d,%d)\n", lcd, LCD_HOME);
#endif
  lcdPutCommand(lcd, LCD_HOME);
  lcd->cx = lcd->cy = 0;
  delay(5);
}

void lcdClear(struct lcdDataStruct *lcd)
{
#ifdef DEBUG
  fprintf(stderr, "lcdClear: lcdPutCommand(%d,%d) and lcdPutCommand(%d,%d)\n", lcd, LCD_CLEAR, lcd, LCD_HOME);
#endif
  lcdPutCommand(lcd, LCD_CLEAR);
  lcdPutCommand(lcd, LCD_HOME);
  lcd->cx = lcd->cy = 0;
  delay(5);
}

/*
 * lcdPosition:
 *	Update the position of the cursor on the display.
 *	Ignore invalid locations.
 *********************************************************************************
 */

void lcdPosition(struct lcdDataStruct *lcd, int x, int y)
{
  // struct lcdDataStruct *lcd = lcds [fd] ;

  if ((x > lcd->cols) || (x < 0))
    return;
  if ((y > lcd->rows) || (y < 0))
    return;

  lcdPutCommand(lcd, x + (LCD_DGRAM | (y > 0 ? 0x40 : 0x00) /* rowOff [y] */));

  lcd->cx = x;
  lcd->cy = y;
}

/*
 * lcdDisplay: lcdCursor: lcdCursorBlink:
 *	Turn the display, cursor, cursor blinking on/off
 *********************************************************************************
 */

void lcdDisplay(struct lcdDataStruct *lcd, int state)
{
  if (state)
    lcdControl |= LCD_DISPLAY_CTRL;
  else
    lcdControl &= ~LCD_DISPLAY_CTRL;

  lcdPutCommand(lcd, LCD_CTRL | lcdControl);
}

void lcdCursor(struct lcdDataStruct *lcd, int state)
{
  if (state)
    lcdControl |= LCD_CURSOR_CTRL;
  else
    lcdControl &= ~LCD_CURSOR_CTRL;

  lcdPutCommand(lcd, LCD_CTRL | lcdControl);
}

void lcdCursorBlink(struct lcdDataStruct *lcd, int state)
{
  if (state)
    lcdControl |= LCD_BLINK_CTRL;
  else
    lcdControl &= ~LCD_BLINK_CTRL;

  lcdPutCommand(lcd, LCD_CTRL | lcdControl);
}

/*
 * lcdPutchar:
 *	Send a data byte to be displayed on the display. We implement a very
 *	simple terminal here - with line wrapping, but no scrolling. Yet.
 *********************************************************************************
 */

void lcdPutchar(struct lcdDataStruct *lcd, unsigned char data)
{
  digitalWrite(gpio, lcd->rsPin, 1);
  sendDataCmd(lcd, data);

  if (++lcd->cx == lcd->cols)
  {
    lcd->cx = 0;
    if (++lcd->cy == lcd->rows)
      lcd->cy = 0;

    // TODO: inline computation of address and eliminate rowOff
    lcdPutCommand(lcd, lcd->cx + (LCD_DGRAM | (lcd->cy > 0 ? 0x40 : 0x00) /* rowOff [lcd->cy] */));
  }
}

/*
 * lcdPuts:
 *	Send a string to be displayed on the display
 *********************************************************************************
 */

void lcdPuts(struct lcdDataStruct *lcd, const char *string)
{
  while (*string)
    lcdPutchar(lcd, *string++);
}

/* ======================================================= */
/* SECTION: aux functions for game logic                   */
/* ------------------------------------------------------- */

/* ********************************************************** */
/* COMPLETE the code for all of the functions in this SECTION */
/* Implement these as C functions in this file                */
/* ********************************************************** */

/* --------------------------------------------------------------------------- */
/* interface on top of the low-level pin I/O code */

/* blink the led on pin @led@, @c@ times */
void blinkN(uint32_t *gpio, int led, int c)
{
  {
    for (int i = 0; i < c; i++)
    {
      writeLED(gpio, led, HIGH);
      delay(550);
      writeLED(gpio, led, LOW);
      delay(550);
    }
  }
}

/* ======================================================= */
/* SECTION: main fct                                       */
/* ------------------------------------------------------- */

int main(int argc, char *argv[])
{ // this is just a suggestion of some variable that you may want to use
  int bits, rows, cols;
  unsigned char func;

  int found = 0, attempts = 0, i, j, code;
  int c, d, buttonPressed, rel, foo;
  int *attSeq;

  int pinLED = LED, pin2LED2 = LED2, pinButton = BUTTON;
  int fSel, shift, pin, clrOff, setOff, off, res;
  int fd;

  int exact, contained;
  char str1[32];
  char str2[32];

  struct timeval t1, t2;
  int t;

  char buf[32];

  // variables for command-line processing
  char str_in[20], str[20] = "some text";
  int verbose = 0, debug = 0, help = 0, opt_m = 0, opt_n = 0, opt_s = 0, unit_test = 0, res_matches = 0;

  // -------------------------------------------------------
  // process command-line arguments

  // see: man 3 getopt for docu and an example of command line parsing
  { // see the CW spec for the intended meaning of these options
    int opt;
    while ((opt = getopt(argc, argv, "hvdus:")) != -1)
    {
      switch (opt)
      {
      case 'v':
        verbose = 1;
        break;
      case 'h':
        help = 1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'u':
        unit_test = 1;
        break;
      case 's':
        opt_s = atoi(optarg);
        break;
      default: /* '?' */
        fprintf(stderr, "Usage: %s [-h] [-v] [-d] [-u <seq1> <seq2>] [-s <secret seq>]  \n", argv[0]);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (help)
  {
    fprintf(stderr, "MasterMind program, running on a Raspberry Pi, with connected LED, button and LCD display\n");
    fprintf(stderr, "Use the button for input of numbers. The LCD display will show the matches with the secret sequence.\n");
    fprintf(stderr, "For full specification of the program see: https://www.macs.hw.ac.uk/~hwloidl/Courses/F28HS/F28HS_CW2_2022.pdf\n");
    fprintf(stderr, "Usage: %s [-h] [-v] [-d] [-u <seq1> <seq2>] [-s <secret seq>]  \n", argv[0]);
    exit(EXIT_SUCCESS);
  }

  if (unit_test && optind >= argc - 1)
  {
    fprintf(stderr, "Expected 2 arguments after option -u\n");
    exit(EXIT_FAILURE);
  }

  if (verbose && unit_test)
  {
    printf("1st argument = %s\n", argv[optind]);
    printf("2nd argument = %s\n", argv[optind + 1]);
  }

  if (verbose)
  {
    fprintf(stdout, "Settings for running the program\n");
    fprintf(stdout, "Verbose is %s\n", (verbose ? "ON" : "OFF"));
    fprintf(stdout, "Debug is %s\n", (debug ? "ON" : "OFF"));
    fprintf(stdout, "Unittest is %s\n", (unit_test ? "ON" : "OFF"));
    if (opt_s)
      fprintf(stdout, "Secret sequence set to %d\n", opt_s);
  }

  seq1 = (int *)malloc(seqlen * sizeof(int));
  seq2 = (int *)malloc(seqlen * sizeof(int));
  cpy1 = (int *)malloc(seqlen * sizeof(int));
  cpy2 = (int *)malloc(seqlen * sizeof(int));

  // check for -u option, and if so run a unit test on the matching function
  if (unit_test && argc > optind + 1)
  { // more arguments to process; only needed with -u
    strcpy(str_in, argv[optind]);
    opt_m = atoi(str_in);
    strcpy(str_in, argv[optind + 1]);
    opt_n = atoi(str_in);
    // CALL a test-matches function; see testm.c for an example implementation
    readSeq(seq1, opt_m); // turn the integer number into a sequence of numbers
    readSeq(seq2, opt_n); // turn the integer number into a sequence of numbers
    if (verbose)
      fprintf(stdout, "Testing matches function with sequences %d and %d\n", opt_m, opt_n);
    int *matches = countMatches(seq1, seq2);
    showMatches(matches, seq1, seq2, 1);
    exit(EXIT_SUCCESS);
  }
  else
  {
    /* nothing to do here; just continue with the rest of the main fct */
  }

  if (opt_s)
  { // if -s option is given, use the sequence as secret sequence
    if (theSeq == NULL)
      theSeq = (int *)malloc(seqlen * sizeof(int));
    readSeq(theSeq, opt_s);
    if (verbose)
    {
      fprintf(stderr, "Running program with secret sequence:\n");
      showSeq(theSeq);
    }
  }

  // -------------------------------------------------------
  // LCD constants, hard-coded: 16x2 display, using a 4-bit connection
  bits = 4;
  cols = 16;
  rows = 2;
  // -------------------------------------------------------

  printf("Raspberry Pi LCD driver, for a %dx%d display (%d-bit wiring) \n", cols, rows, bits);

  if (geteuid() != 0)
    fprintf(stderr, "setup: Must be root. (Did you forget sudo?)\n");

  // init of guess sequence, and copies (for use in countMatches)
  attSeq = (int *)malloc(seqlen * sizeof(int));
  cpy1 = (int *)malloc(seqlen * sizeof(int));
  cpy2 = (int *)malloc(seqlen * sizeof(int));

  // -----------------------------------------------------------------------------
  // constants for RPi2
  gpiobase = 0x3F200000;

  // -----------------------------------------------------------------------------
  // memory mapping
  // Open the master /dev/memory device

  if ((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0)
    return failure(FALSE, "setup: Unable to open /dev/mem: %s\n", strerror(errno));

  // GPIO:
  gpio = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, gpiobase);
  if ((int32_t)gpio == -1)
    return failure(FALSE, "setup: mmap (GPIO) failed: %s\n", strerror(errno));

  // -------------------------------------------------------
  // Configuration of LED and BUTTON

  pinMode(gpio, LED, OUTPUT);
  pinMode(gpio, LED2, OUTPUT);
  pinMode(gpio, BUTTON, INPUT);
  writeLED(gpio, LED, LOW);
  writeLED(gpio, LED2, LOW);

  // -------------------------------------------------------
  // INLINED version of lcdInit (can only deal with one LCD attached to the RPi):
  // you can use this code as-is, but you need to implement digitalWrite() and
  // pinMode() which are called from this code
  // Create a new LCD:
  lcd = (struct lcdDataStruct *)malloc(sizeof(struct lcdDataStruct));
  if (lcd == NULL)
    return -1;

  // hard-wired GPIO pins
  lcd->rsPin = RS_PIN;
  lcd->strbPin = STRB_PIN;
  lcd->bits = 4;
  lcd->rows = rows; // # of rows on the display
  lcd->cols = cols; // # of cols on the display
  lcd->cx = 0;      // x-pos of cursor
  lcd->cy = 0;      // y-pos of curosr

  lcd->dataPins[0] = DATA0_PIN;
  lcd->dataPins[1] = DATA1_PIN;
  lcd->dataPins[2] = DATA2_PIN;
  lcd->dataPins[3] = DATA3_PIN;
  // lcd->dataPins [4] = d4 ;
  // lcd->dataPins [5] = d5 ;
  // lcd->dataPins [6] = d6 ;
  // lcd->dataPins [7] = d7 ;

  // lcds [lcdFd] = lcd ;

  digitalWrite(gpio, lcd->rsPin, 0);
  pinMode(gpio, lcd->rsPin, OUTPUT);
  digitalWrite(gpio, lcd->strbPin, 0);
  pinMode(gpio, lcd->strbPin, OUTPUT);

  for (i = 0; i < bits; ++i)
  {
    digitalWrite(gpio, lcd->dataPins[i], 0);
    pinMode(gpio, lcd->dataPins[i], OUTPUT);
  }
  delay(35); // mS

  // Gordon Henderson's explanation of this part of the init code (from wiringPi):
  // 4-bit mode?
  //	OK. This is a PIG and it's not at all obvious from the documentation I had,
  //	so I guess some others have worked through either with better documentation
  //	or more trial and error... Anyway here goes:
  //
  //	It seems that the controller needs to see the FUNC command at least 3 times
  //	consecutively - in 8-bit mode. If you're only using 8-bit mode, then it appears
  //	that you can get away with one func-set, however I'd not rely on it...
  //
  //	So to set 4-bit mode, you need to send the commands one nibble at a time,
  //	the same three times, but send the command to set it into 8-bit mode those
  //	three times, then send a final 4th command to set it into 4-bit mode, and only
  //	then can you flip the switch for the rest of the library to work in 4-bit
  //	mode which sends the commands as 2 x 4-bit values.

  if (bits == 4)
  {
    func = LCD_FUNC | LCD_FUNC_DL; // Set 8-bit mode 3 times
    lcdPut4Command(lcd, func >> 4);
    delay(35);
    lcdPut4Command(lcd, func >> 4);
    delay(35);
    lcdPut4Command(lcd, func >> 4);
    delay(35);
    func = LCD_FUNC; // 4th set: 4-bit mode
    lcdPut4Command(lcd, func >> 4);
    delay(35);
    lcd->bits = 4;
  }
  else
  {
    failure(TRUE, "setup: only 4-bit connection supported\n");
    func = LCD_FUNC | LCD_FUNC_DL;
    lcdPutCommand(lcd, func);
    delay(35);
    lcdPutCommand(lcd, func);
    delay(35);
    lcdPutCommand(lcd, func);
    delay(35);
  }

  if (lcd->rows > 1)
  {
    func |= LCD_FUNC_N;
    lcdPutCommand(lcd, func);
    delay(35);
  }

  // Rest of the initialisation sequence
  lcdDisplay(lcd, TRUE);
  lcdCursor(lcd, FALSE);
  lcdCursorBlink(lcd, FALSE);
  lcdClear(lcd);

  lcdPutCommand(lcd, LCD_ENTRY | LCD_ENTRY_ID);     // set entry mode to increment address counter after write
  lcdPutCommand(lcd, LCD_CDSHIFT | LCD_CDSHIFT_RL); // set display shift to right-to-left

  // END lcdInit ------
  // -----------------------------------------------------------------------------
  // Start of game
  fprintf(stderr, "Printing welcome message on the LCD display ...\n");
  lcdPosition(lcd, 0, 0);
  lcdPuts(lcd, "Welcome To");
  lcdPosition(lcd, 0, 1);
  lcdPuts(lcd, "MasterMind!!");
  delay(2000);
  /* initialise the secret sequence */
  if (!opt_s)
    initSeq();
  if (debug)
    showSeq(theSeq);
  fprintf(stderr, "%s", "\n");
  // optionally one of these 2 calls:
  // waitForEnter();
  fprintf(stderr, "%s", "Waiting For User To Press Button!!!\n\n");
  lcdClear(lcd);
  lcdPosition(lcd, 0, 0);
  lcdPuts(lcd, "Press Button To Start!!");
  waitForButton(gpio, pinButton);
  fprintf(stderr, "%s", "\n");
  // -----------------------------------------------------------------------------
  // +++++ main loop
  while (attempts != 5)
  {
    // Incrementing the attempts
    attempts++;

    // Red light blinks 3 times at start of each round
    blinkN(gpio, LED2, 3);

    // Displaying the Round Number on LCD
    char round[1];
    sprintf(round, "%d", attempts);
    lcdClear(lcd);
    lcdPosition(lcd, 0, 0);
    lcdPuts(lcd, "Round: ");
    lcdPuts(lcd, round);

    fprintf(stderr, "\n              Round %d           \n", attempts);
    fprintf(stderr, "-----------------------------------\n\n");

    // Variable to store total clicks from button
    int click = 0;

    // Temp Variable to store led output
    int temp = HIGH;

    // Store the Guess
    char guess[30] = {'\0'};

    // Loop for inputing the guess
    for (int i = 0; i < seqlen; i++)
    {

      // Count how many times button was pressed
      buttonPressed = 0;

      // Printing the message
      fprintf(stderr, "             |Guess-%d|\n\n", (i + 1));
      fprintf(stderr, "%s", "            -----------\n");

      // Variable to calculate the time
      int elapsedTime = 0;

      // Input...Process...Output
      while (buttonPressed < seqlen && elapsedTime < 70)
      {
        // Storing whether the button is pressed
        click = readButton(gpio, BUTTON);

        // If the button is pressed
        if (click != 0 && temp == HIGH)
        {
          temp = LOW;
          // Incrementing the buttonPressed
          buttonPressed++;
          fprintf(stderr, "%s", "              Click!!\n");
        }
        // If the button is released then changing the value of temp
        else if (click == 0 && temp == LOW)
        {
          temp = HIGH;
        }

        // Delayig the time
        delay(70);

        // Incrementing the elapsedTime
        elapsedTime++;
      }

      // Printing the message
      fprintf(stderr, "%s", "            -----------\n\n");
      if (buttonPressed == 1)
      {
        fprintf(stderr, "%s", "Guess Made - R.\n");
        strncat(guess, " R", 2);
      }
      else if (buttonPressed == 2)
      {
        fprintf(stderr, "%s", "Guess Made - G.\n");
        strncat(guess, " G", 2);
      }
      else if (buttonPressed == 3)
      {
        fprintf(stderr, "%s", "Guess Made - B.\n");
        strncat(guess, " B", 2);
      }
      else
      {
        fprintf(stderr, "%s", "No Input!!.\n");
        strncat(guess, " #", 2);
      }
      printf("\n");

      // Blinking the red light once as input is accepted
      blinkN(gpio, LED2, 1);

      // Blinking the green light buttonPressed times
      blinkN(gpio, LED, buttonPressed);

      // Putting entered sequence in attseq
      attSeq[i] = buttonPressed;
    }

    // Blinking red LED twice as input of sequence is completed
    blinkN(gpio, LED2, 2);

    fprintf(stderr, "\n        End Of The Round %d        \n", attempts);
    fprintf(stderr, "-----------------------------------\n\n");
    fprintf(stderr, "%s", "\n");

    // Displaying Entered Sequence On LCD and console
    lcdPosition(lcd, 0, 1);
    lcdPuts(lcd, "Guess:");
    lcdPosition(lcd, 6, 1);
    lcdPuts(lcd, guess);
    delay(4000);
    fprintf(stderr, "%s", "\n");
    fprintf(stderr, "%s", "Sequence Entered :");
    showSeq(attSeq);

    // Displaying Exact and approx Matches
    int *match = countMatches(theSeq, attSeq);
    showMatches(match, theSeq, attSeq, 1);
    fprintf(stderr, "%s", "\n");
    lcdClear(lcd);

    // Displaying Exact and approx Matches on LCD
    char exact[1];
    char approx[1];
    sprintf(exact, "%d", match[0]);
    sprintf(approx, "%d", match[1]);
    lcdPosition(lcd, 0, 0);
    lcdPuts(lcd, "Exact: ");
    lcdPosition(lcd, 6, 0);
    lcdPuts(lcd, exact);
    lcdPosition(lcd, 0, 1);
    lcdPuts(lcd, "Approx: ");
    lcdPosition(lcd, 7, 1);
    lcdPuts(lcd, approx);

    // Blinking green LED exact times
    blinkN(gpio, LED, match[0]);

    // Blinking red LED once as a separator
    blinkN(gpio, LED2, 1);

    // Blinking green LED approx times
    blinkN(gpio, LED, match[1]);

    delay(2000);

    // If the exact is same as length then exiting the loop
    if (match[0] == seqlen)
    {
      found = 1;
      break;
    }

    delay(800);
  }

  // If the sequence is found
  if (found == 1)
  {
    lcdClear(lcd);

    // Displaying winning message
    fprintf(stderr, "%s", "\n");
    fprintf(stderr, "Congratulations!!! You won the game\n");
    fprintf(stderr, "%s", "\n\n");

    // Displaying SUCCESS and attempts it took on LCD
    lcdPosition(lcd, 0, 0);
    lcdPuts(lcd, "SUCCESS! ");
    lcdPosition(lcd, 0, 1);
    char att[1];
    sprintf(att, "%d", attempts);
    lcdPuts(lcd, "Attempts:");
    lcdPosition(lcd, 9, 1);
    lcdPuts(lcd, att);

    // Blinking green light 3 times to celebrate victory
    blinkN(gpio, LED, 3);
    delay(300);
  }

  else
  {
    char gen[7]={'\0'};
    lcdClear(lcd);
    for (int i = 0; i < seqlen; i++)
    {
      if (theSeq[i] == 1)
      {
        strncat(gen, " R", 2);
      }
      else if (theSeq[i] == 2)
      {
        strncat(gen, " G", 2);
      }
      else if (theSeq[i] == 3)
      {
        strncat(gen, " B", 2);
      }
      else
      {
        strncat(gen, " #", 2);
      }
      // Displaying Generated Sequence and comforting message
      fprintf(stderr, "%s", "\n");
      fprintf(stderr, "%s", "Generated Sequence :");
      showSeq(theSeq);
      fprintf(stdout, "Well You Will Get It Next Time!!!!\n");
      fprintf(stderr, "%s", "\n\n");

      // Displaying Game Over!! and the generated sequence on LCD
      lcdPosition(lcd, 0, 0);
      lcdPuts(lcd, "Game Over!!");
      lcdPosition(lcd, 0, 1);
      lcdPuts(lcd, "Code Was");
      lcdPosition(lcd, 8, 1);
      lcdPuts(lcd, gen);
      delay(300);
    }

    return 0;
  }
}
