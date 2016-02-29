// UCLA CS 111 Lab 1 command reading
#include "command.h"
#include "command-internals.h"
#include "stdlib.h"
#include "stdio.h"
#include <error.h>

/*----------implementing a stack----------*/
struct node_stack
{
  command_t* commands;
  /* take in pointers to command */
  int top;
};

void stack_init(struct node_stack* stack)
{
  stack->commands = malloc(sizeof(command_t)*100);
  stack->top = 0; /* if top == 0, means it's empty*/
  stack->commands[0] = NULL;
}

int push(command_t s, struct node_stack* stack)
{
  (stack->commands)[++(stack->top)] = s;
  return stack->top; /* return the size */ 
}

command_t pop(struct node_stack* stack)
{
  /* assume that it's not empty */
  if(stack->top > 0)
    return stack->commands[stack->top--];
  else
    return NULL;
}

command_t top(struct node_stack* stack)
{
  if(stack->top > 0)
    return stack->commands[stack->top];
  else
    return NULL;
}

int stack_size(struct node_stack* stack)
{
  return stack->top;
}

/*----------the command stream----------*/
struct command_stream
{
  int current;
  command_t* single_commands;
};

void
recover_stream(command_stream_t s)
{
  s->current = 0;
}

/*----------command tree assembler and it's helpers----------*/
int
is_precedence_greater(command_t left, command_t right)
{/* return 1 is left is greater than right, 0 otherwise */
  enum command_type l = left->type;
  enum command_type r = right->type;
  if(l == PIPE_COMMAND)
    {
      if(r == PIPE_COMMAND)
        return 0;
      else
        return 1;
    }
  else if(l == AND_COMMAND || l == OR_COMMAND)
    {
      if(r == PIPE_COMMAND || r == AND_COMMAND || r == OR_COMMAND)
	return 0;
      else
	return 1;
    }
  else
    return 0;
}

command_t
combine(command_t o, command_t left, command_t right)
{
  o->u.command[0] = left;
  o->u.command[1] = right;
  return o;
}

command_t
assembleCommand(command_t* commands)
{/* implementing TUAN's ALGORITHM */
  struct node_stack cStack;
  struct node_stack oStack;
  command_t o_top;
  command_t o;
  command_t first;
  command_t sec;
  command_t c;
  stack_init(&cStack);
  stack_init(&oStack);

  while(*commands)
    {
      c = *(commands++);
      if(c->type == SIMPLE_COMMAND || c->type == SUBSHELL_COMMAND)
        push(c, &cStack); /* if not SIMPLE_COMMAND, then an operator */
      else if(!stack_size(&oStack))
	push(c, &oStack);
      else if(is_precedence_greater(c, top(&oStack)))
	push(c, &oStack);
      else
	{
	  o_top = top(&oStack);
	  while(!is_precedence_greater(c, o_top))
	    {
	      o = pop(&oStack);
	      sec = pop(&cStack);
	      first = pop(&cStack);
	      if(!first || !sec)
		error(1, 0, "%d: missing operand", o->line_num);
	      push(combine(o, first, sec), &cStack);
	      if(!stack_size(&oStack))
		break;
	      else
		o_top = top(&oStack);
	    }
	  push(c, &oStack);
	}
    }
  /* after the loop, ther oStack have all same precedence operators... */
  while(stack_size(&oStack))
    {
      o = pop(&oStack);
      sec = pop(&cStack);
      first = pop(&cStack);
      if(!first || !sec)
	error(1, 0, "%d: missing operand", o->line_num);
      push(combine(o, first, sec), &cStack);
    }
  if(stack_size(&cStack) > 1)
    {
      c = pop(&cStack);
      c = pop(&cStack);
      error(1, 0, "%d: too many operand", c->line_num);
    }
  c = pop(&cStack);
  return c;
}

/*----------tokenizer and it's helpers----------*/
/* Subshell is very unclear: for now assume that
   Subshell forms a single command, and will not
   become part of another command tree.*/
struct tokenizer_attribute
{
  int left;
  int right;
  int count;
  int word_count;
  int missing_operand;
  int next_word_input;
  int next_word_output;
  int open_parenthesis;
  int i;
  int* table;
  char* input;
  char* output;
  char** word;
};

void
tokenizer_init(struct tokenizer_attribute* u)
{
  u->left = 0;
  u->right = 0;
  u->count = 0;
  u->word_count = 0;
  u->missing_operand = 0;
  u->next_word_input = 0;
  u->next_word_output = 0;
  u->open_parenthesis = 0;
  u->i = 0;
  u->table = NULL;
  u->input = NULL;
  u->output = NULL;
  u->word = NULL;
}

void
finish_word(char* buffer, struct tokenizer_attribute* u, int LIST_CAP)
{
  int BUFFER_CAP = 20;
  char* ptr = malloc(sizeof(char)*BUFFER_CAP);
  int i = 0;
  while(u->left <= u->right)
    {
      char c = buffer[(u->left)++];
      if(c != ' ' && c != '\t' && c != '\n')
	ptr[i++] = c;
      if(i >= BUFFER_CAP)
	{
	  BUFFER_CAP *= 2;
	  ptr = realloc(ptr, sizeof(char)*BUFFER_CAP);
	}
    }
  u->right = u->left;
  ptr[i] = '\0';

  if(u->next_word_input && u->next_word_output)
    error(1, 0, "%d: missing input/output operand", u->table[u->i]);
  else if(u->next_word_input)
    {
      if(ptr == '\0')
	error(1, 0, "%d: missing input", u->table[u->i]);
      u->input = ptr;
      u->next_word_input = 0;
    }
  else if(u->next_word_output)
    {
      if(ptr == '\0')
	error(1, 0, "%d: missing output", u->table[u->i]);
      u->output = ptr;
      u->next_word_output = 0;
    }
  else
    {
      if(u->word == NULL)
	u->word = malloc(sizeof(char*)*LIST_CAP);
      
      (u->word)[(u->word_count)++] = ptr;
      (u->word)[u->word_count] = NULL;
    }
  return;
}

void
finish_previous(command_t* result, struct tokenizer_attribute* u)
{

  if(u->count > 0 && result[(u->count)-1]->type == SUBSHELL_COMMAND)
    {// subshell also can have i/o ! shoot!
      if(u->word != NULL)
	error(1, 0, "%d: too many operand", u->table[u->i]);
      result[(u->count)-1]->input = u->input;
      u->input = NULL;
      result[(u->count)-1]->output = u->output;
      u->output = NULL;
      return;
    }
  
  command_t ptr = malloc(sizeof(struct command));
  ptr->type = SIMPLE_COMMAND;
  ptr->line_num = u->table[u->i];

  if(u->word == NULL)
    error(1, 0, "%d: missing operand", u->table[u->i]);
  ptr->u.word = u->word; /* u.word is the union field of command struct */
  u->word = NULL;
  u->word_count = 0;
  
  ptr->input = u->input;
  u->input = NULL;
  
  ptr->output = u->output;
  u->output = NULL;

  result[(u->count)++] = ptr;
  return;
}


command_t*
tokenizer(char* buffer, int start, int end, int* table)
{
  int LIST_CAP = 20;
  command_t* result = malloc(sizeof(command_t)*LIST_CAP); 
  struct tokenizer_attribute u;
  tokenizer_init(&u);
  u.table = table;
  u.left = start;
  u.right = u.left-1;

  while(buffer[start] == ' ' ||	buffer[start] == '\t' || buffer[start] =='\n')
    start++;
  for(u.i=start; u.i<end; u.i++)
    {
      char c = buffer[u.i];
      if(c == '|')
	{
	  /* previous is SIMPLE_COMMAND, and must have word as operand*/
	  if(u.right >= u.left)
	    finish_word(buffer, &u, LIST_CAP);
	  finish_previous(result, &u);
	  u.missing_operand = 1;
	      
	  if(buffer[u.i+1] == '|')
	    {/* current is OR_COMMAND */
	      command_t ptr = malloc(sizeof(struct command));
	      ptr->type = OR_COMMAND;
	      ptr->input = NULL;
	      ptr->output = NULL;
	      ptr->line_num = table[u.i];
	      result[u.count++] = ptr;
	      u.i++;
	      u.left = u.i+1;
	    }
	  else
	    {/* current is PIPE_COMMAND */
	      command_t ptr = malloc(sizeof(struct command));
	      ptr->type = PIPE_COMMAND;
	      ptr->input = NULL;
	      ptr->output = NULL;
	      ptr->line_num = table[u.i];
	      result[u.count++] = ptr;
	      u.left = u.i+1;
	    }
	}
      else if(c == '&')
	{
	  /* previous is SIMPLE_COMMAND, and must have word as operand */
	  if(u.right >= u.left)
	    finish_word(buffer, &u, LIST_CAP);
	  finish_previous(result, &u);
	  u.missing_operand = 1;
	      
	  if(buffer[u.i+1] == '&')
	    {/* current is AND_COMMAND */
	      command_t ptr = malloc(sizeof(struct command));
	      ptr->type = AND_COMMAND;
	      ptr->input = NULL;
	      ptr->output = NULL;
	      ptr->line_num = table[u.i];
	      result[u.count++] = ptr;
	      u.i++;
	      u.left = u.i+1;
	    }
	  else
	    error(1, 0, "%d: unsupported operator '&'", table[u.i]);
	}
      else if(c == ';' || (c == '\n' && !u.missing_operand))
	{/* current is SEQUENCE_COMMAND, previous is SIMPLE_COMMAND */
	  if(u.right >= u.left)
	      finish_word(buffer, &u, LIST_CAP);
	  if(u.i > start) /* if it's the start, no previous command */
	    finish_previous(result, &u);

	  command_t ptr = malloc(sizeof(struct command));
	  ptr->type = SEQUENCE_COMMAND;
	  ptr->input = NULL;
	  ptr->output = NULL;
	  ptr->line_num = table[u.i];
	  result[u.count++] = ptr;
	  u.left = u.i+1;
 	}
      else if(c == '(')
	{/* previous shouldn't have anything, current is SUBSHELL_COMMAND */
	  if(u.right >= u.left)
	    error(1, 0, "%d: unexpected token '('", table[u.i]);
	  u.missing_operand = 0;
	  command_t ptr = malloc(sizeof(struct command));
	  ptr->type = SUBSHELL_COMMAND;
	  ptr->input = NULL;
	  ptr->output = NULL;
	  ptr->line_num = table[u.i];
	  u.open_parenthesis++;

	  int sub_shell_left = u.i+1;
	  int sub_shell_right = sub_shell_left;
	  while(sub_shell_right < end)
	    {
	      char sub_c = buffer[sub_shell_right++];
	      if(sub_c == ')')
		{
		  u.open_parenthesis--;
		  if(u.open_parenthesis == 0)
		    break;
		}
	      else if(sub_c == '(')
		u.open_parenthesis++;
	    }
	  if(u.open_parenthesis != 0)
	    error(1, 0, "%d: not matching parenthesis", table[u.i]);
	  /* sub_shell_right is pointing one after the ')' */
	  u.left = sub_shell_right--;
	  u.i = sub_shell_right;
	  command_t* sub_shell = tokenizer(buffer, sub_shell_left,
					   sub_shell_right, table);
	  /* since the print function just prints subshell commands,
	     treat this as error to avoid segmentation fault*/
	  if(sub_shell[0] == NULL)
	    error(1, 0, "%d: empty sub shell", table[u.i]);
	  ptr->u.subshell_command = assembleCommand(sub_shell);
	  result[u.count++] = ptr;

	}
      else if(c == ')')
	error(1, 0, "%d: not matching parenthesis", table[u.i]);
      else if(c == '<')
	{
	  if(u.right >= u.left)
	    finish_word(buffer, &u, LIST_CAP);
	  u.left = u.i+1;
	  
	  if(u.input != NULL || u.next_word_input)
	    error(1, 0, "%d: multiple input!", table[u.i]);
	  else
	    {
	      u.next_word_input = 1;
	      u.missing_operand = 1;
	      u.left = u.i+1;
	    }
	}
      else if(c == '>')
	{
	  if(u.right >= u.left)
	    finish_word(buffer, &u, LIST_CAP);
	  u.left = u.i+1;
	  
	  if(u.output != NULL || u.next_word_output)
	    error(1, 0, "%d: multiple output!", table[u.i]);
	  else
	    {
	      u.next_word_output = 1;
	      u.missing_operand = 1;
	      u.left = u.i+1;
	    }
	}
      else if(c == ' ' || c == '\t' || c == '\n')
	{/* finish the word */
	  if(u.right >= u.left)
	    finish_word(buffer, &u, LIST_CAP);
	  u.left = u.i+1;
	}
      else
	{
	  u.right = u.i;
	  u.missing_operand = 0;
	}

      if(u.count >= LIST_CAP || u.word_count >= LIST_CAP)
	{
	  LIST_CAP *= 2;
	  result = realloc(result, sizeof(void*)*LIST_CAP);
	  u.word = realloc(u.word, sizeof(void*)*LIST_CAP);
	}
      
    }
  if(u.missing_operand)
    error(1, 0, "%d: missing operand", table[u.i]);
  if(u.right >= u.left)
    finish_word(buffer, &u, LIST_CAP);
  if(u.word || u.input || u.output)
    finish_previous(result, &u);
  if(u.count > 0 && result[u.count-1]->type == SEQUENCE_COMMAND)
    u.count--;
  result[u.count++] = '\0';
  return result;
}

/*----------make command stream from file stream----------*/
command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  int ARRAY_CAP = 5;
  int BUFFER_CAP = 50;
  int last_is_newline = 0;
  int missing_operand = 0;
  char c = get_next_byte(get_next_byte_argument);
  int nCommand = 0;
  int index = 0;
  int* end_index = malloc(sizeof(int)*ARRAY_CAP);
  /* keep track of the end index of the command string in cBuffer */
  char* cBuffer = malloc(sizeof(char)*BUFFER_CAP); /* record the command string */
  command_t* command_array = malloc(sizeof(command_t)*ARRAY_CAP);

  while(c != EOF)
    {/* store in buffer */
      if(index >= BUFFER_CAP)
	{
	  BUFFER_CAP *= 2;
	  cBuffer = realloc(cBuffer, sizeof(char)*BUFFER_CAP);
	}
      cBuffer[index++] = c;

      if(c == '\n' && !missing_operand)
	{ /* end the single command if we have 2 consecutive newline */
	  if(last_is_newline)
	    { /* finished a single command */
	      if(nCommand >= ARRAY_CAP)
		{
		  ARRAY_CAP *= 2;		  
		  end_index = realloc(end_index, sizeof(int)*ARRAY_CAP);
		  command_array = realloc(command_array, sizeof(void*)*ARRAY_CAP);
		}
	      end_index[nCommand] = index-2;
	      /* treat the 2 newlines as leading newline */

	      command_array[nCommand++] = malloc(sizeof(struct command)*1);
	      last_is_newline = 0;
	    }
	  else
	    last_is_newline = 1;
	}
      else
	{
	  if(c == '#' && 
	     (index == 1 || index == end_index[nCommand-1]+1 ||
	      cBuffer[index-2] == ' ' || cBuffer[index-2] == '\t' ||
	      cBuffer[index-2] == '\n') )
	    {
	      index--;
	      while(c != '\n')
		c = get_next_byte(get_next_byte_argument);
	      continue;
	    }
	  last_is_newline = 0;

	  /* check if it's an operator or a word*/
	  if(c == '|' || c == '&')
	    missing_operand = 1;
	  else if(c != ' ' && c != '\t' && c != '\n')
	    missing_operand = 0;
	}
      c = get_next_byte(get_next_byte_argument);
    }
  
  if(nCommand >= ARRAY_CAP)
    {
      ARRAY_CAP *= 2;		  
      end_index = realloc(end_index, sizeof(int)*ARRAY_CAP);
      command_array = realloc(command_array, sizeof(void*)*ARRAY_CAP);
    }
  end_index[nCommand] = index;
  /* always points to one after the final char of the last command. */
  command_array[nCommand++] = malloc(sizeof(struct command)*1);
  
  /* a hash table for index<->line_number */
  int length = end_index[nCommand-1];
  int* line_number_table = malloc(sizeof(int)*length);
  int i=0;
  int count=1;
  for(i = 0; i<length; i++)
    {
      char c = cBuffer[i];
      line_number_table[i] = count;
      if(c == '\n')
	count++;
      else if(c == '\'' || c == '"' || c == '`')
	error(1, 0, "%d: unsupported quote `'\"", count);
    }

  i = 0;
  int j=0;
  int start=0;
  while(i < nCommand)
    {
      command_t* toAssemble =
	tokenizer(cBuffer, start, end_index[i], line_number_table);
      if(toAssemble[0] == NULL)	;
      else if(toAssemble[1] == NULL) /* there is only a single command */
	command_array[j++] = toAssemble[0];
      else
	command_array[j++] = assembleCommand(toAssemble);
      start = end_index[i++];
    }
  if(nCommand >= ARRAY_CAP)
    {
      ARRAY_CAP *= 2;
      command_array = realloc(command_array, sizeof(void*)*ARRAY_CAP);
    }
  command_array[j] = NULL; /* append a NULL ptr to the end. */

  command_stream_t stream = malloc(sizeof(struct command_stream));
  stream->single_commands = command_array;
  stream->current = 0;
  return stream;
}

/*----------read a command from a stream----------*/
command_t
read_command_stream (command_stream_t s)
{
  return (s->single_commands)[(s->current)++];
}
