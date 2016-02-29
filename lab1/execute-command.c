// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

int** graph;

struct file_pool
{
  char** read;
  char** write;
  int r_top;
  int w_top;
};

typedef struct file_pool* file_pool_t;

void pool_init(file_pool_t pool)
{
  pool->read = malloc(sizeof(char*)*100); /*assuming it's enough*/
  pool->write = malloc(sizeof(char*)*100);
  pool->r_top = 0; /* top == 0, means empty */
  pool->w_top = 0;
  pool->read[0] = NULL;
  pool->write[0] = NULL;
}

int pool_add_write(file_pool_t pool, char* file_name)
{
  (pool->write)[(pool->w_top)++] = file_name;
  return pool->w_top; /* return number of write */
}

int pool_add_read(file_pool_t pool, char* file_name)
{
  (pool->read)[(pool->r_top)++] = file_name;
  return pool->r_top;
}

int pool_check_same(char** left_pool, char** right_pool)
{
  /* return 1 if there are same names, 0 otherwise */
  char** right_ptr = right_pool;
  while(*right_ptr)
    {
      char** left_ptr = left_pool;
      while(*left_ptr)
	{
	  if(!strcmp(*(left_ptr++), *right_ptr))
	    return 1;
	}
      right_ptr++;
    }
  return 0;
}

void fill_pool(file_pool_t pool, command_t c)
{
  if(c->input)
    pool_add_read(pool, c->input);
  if(c->output)
    pool_add_write(pool, c->output);
  if(c->type == SIMPLE_COMMAND)
    {
      int count = 1;
      while( (c->u.word)[count] )
	pool_add_read(pool, (c->u.word)[count++]);
    }
  else if(c->type == SUBSHELL_COMMAND)
      fill_pool(pool, c->u.subshell_command);
  else
    {
      fill_pool(pool, c->u.command[0]);
      fill_pool(pool, c->u.command[1]);
    }
  return;
}

int check_dependency(file_pool_t left, file_pool_t right)
{
  /* return 1 if right depends on left, 0 otherwise */
  int result = 0;
  /* read after write */
  char** right_ptr = right->read;
  char** left_ptr = left->write;
  if( pool_check_same(left_ptr, right_ptr) )
    return 1;
  /* writr after write */
  right_ptr = right->write;
  if( pool_check_same(left_ptr, right_ptr) )
    return 1;
  /* write after read */
  left_ptr = left->read;
  if( pool_check_same(left_ptr, right_ptr) )
    return 1;

  return 0;
}

int**
make_graph(command_stream_t stream)
{
  /* ASSUMPTION: only write to file in output '>', and read from
     every word in the command, and input '<'. */

  /* TODO: free the file pool! (loop free) */
  /* TODO: dynamically allocate file pools */

  /*int** graph = malloc(sizeof(int*)*100); *//* assuming it's enough */
  graph = mmap(NULL, sizeof(int*)*100, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  file_pool_t command_pool[100];
  int count = 0;
  
  command_t c = read_command_stream(stream);
  while(c)
    {
      /* add a row in the graph, and add a file_pool */
      /*graph[count] = malloc(sizeof(int)*100);*/
      graph[count] = mmap(NULL, sizeof(int)*100, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
      command_pool[count] = malloc(sizeof(struct file_pool));
      /* fill the file_pool */
      pool_init(command_pool[count]);
      fill_pool(command_pool[count], c);
      /* update step */
      c = read_command_stream(stream);
      count++;
    }
  graph[count] = NULL;
  command_pool[count] = NULL;

  int i, j;
  for(i=0; i<count-1; i++)
    {
      for(j=i+1; j<count; j++)
	{
	  if( check_dependency(command_pool[i], command_pool[j]) )
	    graph[j][i] = 1;
	  else
	    graph[j][i] = 0;
	}
    }
  /* first row are all 0s, since first command doesnt depend on anything
     and append -1 after the last column for each row. */
  for(j=0; j<count; j++)
    graph[0][j] = 0;
  for(i=0; i<count; i++)
    graph[i][count] = -1;

  return graph;
}

/* execute_graph by Yunong */

void
execute_graph(int** graph, command_stream_t stream, int N)
{
  int i,j,n = 0, m, np = N;
  while (graph[n]) n++;

  command_t* command_stream = malloc(sizeof(command_t)*n);
  int count = 0, status;
  while(count < n)
      command_stream[count++] = read_command_stream(stream);

  int *executed = malloc(sizeof(int)*n);

  for (i = 0; i<n; i++)
    executed[i] = 0;
  m = n;
  while (m > 0)
    {
      for (i = 0; i < n; i++)
	if (!executed[i])
	  {
	    int dependency = 0;
	    for (j = 0; j < n; j++)
	      if (graph[i][j]) dependency = 1;
	    if (!dependency)
	      {
		if (np == 0) // if the limit of subprocesses is met 
		  {
		    waitpid(WAIT_ANY, &status, WUNTRACED); // wait for any child to return
		    if (errno == EINTR || errno == EINVAL) perror("waitpid");
		    np = 1;
		  } // the check is put right before forking, so that the efficiency is maximized.
		m--; np--;
		executed[i] = 1;
		pid_t pid = fork();
		if (!pid)
		  {/* child process execute the command */
		    execute_command(command_stream[i], 0);
		    for (j = 0; j < n; j++)
		      graph[j][i] = 0;
		    exit(command_status(command_stream[i]));
		  }
	      }
	  }
    }
 
  while (waitpid(WAIT_ANY, &status, WUNTRACED))
     { 
       if (errno == ECHILD) break; 
       if (errno == EINTR || errno == EINVAL) perror("waitpid");
     }
}


int
command_status (command_t c)
{
  return c->status;
}

void
execute_command (command_t c, int time_travel)
{
  int child_pid, status, outputfd, inputfd, command_pipe[2];
  if(!time_travel)
    {
      switch(c->type)
	{
	case AND_COMMAND:
	case OR_COMMAND:
	  {
	    execute_command(c->u.command[0], 0);
	    if( ( (c->type == AND_COMMAND) && 
		  !(c->status = c->u.command[0]->status) ) ||
		( (c->type == OR_COMMAND) && 
		  (c->status = c->u.command[0]->status) ) )
	      {
		execute_command(c->u.command[1], 0);
		c->status = c->u.command[1]->status;
	      }
	    break;
	  }
	case SEQUENCE_COMMAND:
	  {
	    execute_command(c->u.command[0], 0);
	    execute_command(c->u.command[1], 0);
	    c->status = c->u.command[1]->status;
	    break;
	  }
	case PIPE_COMMAND:
	  {
	    if(pipe(command_pipe) < 0)
	      error(1, 0, "fail to create pipe.");
	    if( (child_pid = fork()) < 0)
	      error(1, 0, "fail to fork child.");
	    if(child_pid == 0)
	      {// first child: write to pipe
		close(command_pipe[0]);
		if(dup2(command_pipe[1], 1) < 0)
		  error(1, 0, "fail to setup redirection.");
		close(command_pipe[1]);
		execute_command(c->u.command[0], 0);
		_exit(c->u.command[0]->status);
	      }
	    else
	      {
		close(command_pipe[1]);
		if( (child_pid = fork()) < 0)
		  error(1, 0, "fail to fork child.");
		if(child_pid == 0)
		  {// second child: read from pipe
		    if(dup2(command_pipe[0], 0) < 0)
		      error(1, 0, "fail to setup redirection.");
		    close(command_pipe[0]);
		    execute_command(c->u.command[1], 0);
		    waitpid(child_pid, &status, 0);
		    _exit(c->u.command[1]->status);
		  }
		else
		  {// wait for second child
		    close(command_pipe[0]);
		    waitpid(child_pid, &status, 0);
		    c->status = WEXITSTATUS(status);
		  }
	      }
	    break;
	  }
	case SUBSHELL_COMMAND:
	case SIMPLE_COMMAND:
	  {
	    if( (child_pid = fork())<0 )
	      error(1, 0, "fail to fork child.");
	    if(child_pid == 0)
	      {
		if(c->input)
		  {
		    if( (inputfd = 
			 open(c->input, O_RDONLY))<0 )
		      error(1, 0, "%s: no such file or directory.", c->input);
		    if( dup2(inputfd, 0)<0 )
		      error(1, 0, "fail to setup redirection.");
		    close(inputfd);
		  }
		if(c->output)
		  {
		    if( (outputfd = 
			 open(c->output, O_WRONLY|O_CREAT,
			      S_IWUSR|S_IRUSR))<0 )
		      error(1, 0, "%s: cannot open file.", c->output);
		    if(dup2(outputfd, 1) < 0)
		      error(1, 0, "fail to setup redirection.");
		    close(outputfd);
		  }
		if(c->type == SIMPLE_COMMAND)
		  {
		    if( execvp(c->u.word[0], c->u.word)<0 )
		      error(1, 0, "%s: fail to execute command", c->u.word[0]);
		  }
		else
		  {
		    execute_command(c->u.subshell_command, 0);
		    _exit(c->u.subshell_command->status);
		  }
	      }
	    else
	      {
		waitpid(child_pid, &status, 0);
		c->status = WEXITSTATUS(status);
	      }
	    break;
	  }
	default:
	  error(1, 0, "unrecognized command type.");
	  break;
	}
    }
  return;
}
