/*
  A C program to test the matching function (for master-mind) as implemented in matches.s
//mm-matches.s is not used
$ as  -o mm-matches.o mm-matches.s
$ gcc -c -o testm.o testm.c
$ gcc -o testm testm.o matches.o
$ ./testm
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define LENGTH 3
#define COLORS 3

#define NAN1 8
#define NAN2 9

const int seqlen = LENGTH;
const int seqmax = COLORS;

/* ********************************** */
/* take these fcts from master-mind.c */
/* ********************************** */

/* Show given sequence */
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

/* Parse an integer value as a list of digits of base MAX */
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




#define NAN1 8
#define NAN2 9

/* counts how many entries in seq2 match entries in seq1 */
/* returns exact and approximate matches, either both encoded in one value, */
/* or as a pointer to a pair of values */
int *countMatches(int *seq1, int *seq2)
{
  // Initializing the counters for exact and approx matches
  int exact = 0;
  int approx = 0;

  // Allocating memory for an array to mark visited elements
  int *visited = (int *)malloc(seqlen * sizeof(int));

  // Iterating through array and setting each element to 0 to mark as not visited
  for (int i = 0; i < seqlen; i++)
  {
    visited[i] = 0;
  }

  // Iterating through both sequences to find exact matches
  for (int i = 0; i < seqlen; i++)
  {
    // Finding exact matches
    if (seq1[i] == seq2[i])
    {
      // Incrementing the exact and marking that position as visited
      exact++;
      visited[i] = 1;
    }
  }

  // Iterating through both sequences to find approximate matches
  for (int i = 0; i < seqlen; i++)
  {
    // Checking if the elements at the same position are not equal
    if (seq1[i] != seq2[i])
    {
      // Iterating through seq1 to find a matching element
      for (int j = 0; j < seqlen; j++)
      {
        // Checking if an element in seq1 matches the current element in seq2 and has not been visited
        if (seq2[i] == seq1[j] && !visited[j])
        {
          // Incrementing the approx matches and marking the element in seq1 as visited
          approx++;
          visited[j] = 1;
          break;
        }
      }
    }
  }

  // Allocateing memory for the output array
  int *output = malloc(2 * sizeof(int));
  // Storing the matches in array
  output[0] = exact;
  output[1] = approx;

  free(visited); // Freeing memory allocated for the visited array

  return output; // Returning output
}

void showMatches(int *code, /* only for debugging */ int *seq1, int *seq2, /* optional, to control layout */ int lcd_format)
{
  // Printing Exact and Approx Matches
  printf("Exact : %d\n", code[0]);
  printf("Approx : %d\n", code[1]);
}

/* read a guess sequence fron stdin and store the values in arr */
/* only needed for testing the game logic, without button input */
int readNum(int max); 

// The ARM assembler version of the matching fct
int* matches(int *seq1, int *seq2){
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

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int main (int argc, char **argv) {
  int res, res_c, t, t_c, m, n;
  int *seq1, *seq2, *cpy1, *cpy2;
  struct timeval t1, t2 ;
  char str_in[20], str[20] = "some text";
  int verbose = 0, debug = 0, help = 0, opt_s = 0, opt_n = 0;
  
  // see: man 3 getopt for docu and an example of command line parsing
  { // see the CW spec for the intended meaning of these options
    int opt;
    while ((opt = getopt(argc, argv, "hvs:n:")) != -1) {
      switch (opt) {
      case 'v':
	verbose = 1;
	break;
      case 'h':
	help = 1;
	break;
      case 'd':
	debug = 1;
	break;
      case 's':
	opt_s = atoi(optarg); 
	break;
      case 'n':
	opt_n = atoi(optarg); 
	break;
      default: /* '?' */
	fprintf(stderr, "Usage: %s [-h] [-v] [-s <seed>] [-n <no. of iterations>]  \n", argv[0]);
	exit(EXIT_FAILURE);
      }
    }
  }

  seq1 = (int*)malloc(seqlen*sizeof(int));
  seq2 = (int*)malloc(seqlen*sizeof(int));
  cpy1 = (int*)malloc(seqlen*sizeof(int));
  cpy2 = (int*)malloc(seqlen*sizeof(int));
  
  if (argc > optind+1) {
    strcpy(str_in, argv[optind]);
    m = atoi(str_in);
    strcpy(str_in, argv[optind+1]);
    n = atoi(str_in);
    fprintf(stderr, "Testing matches function with sequences %d and %d\n", m, n);
  } else {
    int i, j, n = 10, res, res_c, oks = 0, tot = 0; // number of test cases
    fprintf(stderr, "Running tests of matches function with %d pairs of random input sequences ...\n", n);
    if (opt_n != 0)
      n = opt_n;
    if (opt_s != 0)
      srand(opt_s);
    else
      srand(1701);
    for (i=0; i<n; i++) {
      for (j=0; j<seqlen; j++) {
	seq1[j] = (rand() % seqlen + 1);
	seq2[j] = (rand() % seqlen + 1);
      }
      memcpy(cpy1, seq1, seqlen*sizeof(int));
      memcpy(cpy2, seq2, seqlen*sizeof(int));
      if (verbose) {
	fprintf(stderr, "Random sequences are:\n");
	showSeq(seq1);
	showSeq(seq2);
      }
      res = matches(seq1, seq2);         // extern; code in matches.s
      memcpy(seq1, cpy1, seqlen*sizeof(int));
      memcpy(seq2, cpy2, seqlen*sizeof(int));
      res_c = countMatches(seq1, seq2);  // local C function
      if (debug) {
	fprintf(stdout, "DBG: sequences after matching:\n");	
	showSeq(seq1);
	showSeq(seq2);
      }
      fprintf(stdout, "Matches (encoded) (in C):   %d\n", res_c);
      fprintf(stdout, "Matches (encoded) (in Asm): %d\n", res);
      memcpy(seq1, cpy1, seqlen*sizeof(int));
      memcpy(seq2, cpy2, seqlen*sizeof(int));
      showMatches(res_c, seq1, seq2, 0);
      showMatches(res, seq1, seq2, 0);
      tot++;
      if (res == res_c) {
	fprintf(stdout, "__ result OK\n");
	oks++;
      } else {
	fprintf(stdout, "** result WRONG\n");
      }
    }
    fprintf(stderr, "%d out of %d tests OK\n", oks, tot);
    exit(oks==tot ? 0 : 1);
  }    

  readSeq(seq1, m);
  readSeq(seq2, n);

  memcpy(cpy1, seq1, seqlen*sizeof(int));
  memcpy(cpy2, seq2, seqlen*sizeof(int));
  memcpy(seq1, cpy1, seqlen*sizeof(int));
  memcpy(seq2, cpy2, seqlen*sizeof(int));
    
  gettimeofday (&t1, NULL) ;
  res_c = countMatches(seq1, seq2);         // local C function
  gettimeofday (&t2, NULL) ;
  // d = difftime(t1,t2);
  if (t2.tv_usec < t1.tv_usec)	// Counter wrapped
    t_c = (1000000 + t2.tv_usec) - t1.tv_usec;
  else
    t_c = t2.tv_usec - t1.tv_usec ;

  if (debug) {
    fprintf(stdout, "DBG: sequences after matching:\n");	
    showSeq(seq1);
    showSeq(seq2);
  }
  memcpy(seq1, cpy1, seqlen*sizeof(int));
  memcpy(seq2, cpy2, seqlen*sizeof(int));
  
  gettimeofday (&t1, NULL) ;
  res = matches(seq1, seq2);         // extern; code in hamming4.s
  gettimeofday (&t2, NULL) ;
  // d = difftime(t1,t2);
  if (t2.tv_usec < t1.tv_usec)	// Counter wrapped
    t = (1000000 + t2.tv_usec) - t1.tv_usec;
  else
    t = t2.tv_usec - t1.tv_usec ;

  if (debug) {
    fprintf(stdout, "DBG: sequences after matching:\n");	
    showSeq(seq1);
    showSeq(seq2);
  }

  memcpy(seq1, cpy1, seqlen*sizeof(int));
  memcpy(seq2, cpy2, seqlen*sizeof(int));
  showMatches(res_c, seq1, seq2, 0);
  showMatches(res, seq1, seq2, 0);

  if (res == res_c) {
    fprintf(stdout, "__ result OK\n");
  } else {
    fprintf(stdout, "** result WRONG\n");
  }
  fprintf(stderr, "C   version:\t\tresult=%d (elapsed time: %dms)\n", res_c, t_c);
  fprintf(stderr, "Asm version:\t\tresult=%d (elapsed time: %dms)\n", res, t);


  return 0;
}
