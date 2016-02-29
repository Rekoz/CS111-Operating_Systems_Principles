// UCLA CS 111 Lab 1 main program

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "command.h"

static char const *program_name;
static char const *script_name;

static void
usage (void)
{
  error (1, 0, "usage: %s [-ptn:] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

int
main (int argc, char **argv)
{
  int opt;
  int command_number = 1;
  int print_tree = 0;
  int time_travel = 0;
  int N = INT_MAX;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "ptn:")) // option n takes a required argument
      {
      case 'p': print_tree = 1; break;
      case 't': time_travel = 1; break;
      case 'n': 
	{
	  time_travel = 1; 
	  N = atoi(optarg);
	  if (N<1)
	    error (1, 0, "number of subprocesses must be larger than 0");
	  break; // if user inputs option n, we assume that parallelism is activated
	}
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  command_t last_command = NULL;
  command_t command;
  /* modify main to implement time travel */
  if(time_travel)
    {
      int** graph = make_graph(command_stream);
      int* ptr;
      int count = 0;
      /*while( (ptr = graph[count++]) )
	{
	  while(*ptr != -1)
	    printf("%d ", *(ptr++));
	  printf("\n");
	  }*/
      /* need to recover the stream! */
      recover_stream(command_stream);
      execute_graph(graph, command_stream, N);
      return 0;
    }
  /* end modification */
  while ((command = read_command_stream (command_stream)))
    {
      if (print_tree)
	{
	  printf ("# %d\n", command_number++);
	  print_command (command);
	}
      else
	{
	  last_command = command;
	  execute_command (command, time_travel);
	}
    }

  return print_tree || !last_command ? 0 : command_status (last_command);
}
