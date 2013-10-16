Build Your Own Lisp in 1000 Lines of C
======================================

Introduction
------------

In this tutorial I'll show you how to build a minimal Lisp in just a few lines of C. Okay so I'm cheating a little bit because we'll be using my Parser Combinator library `mpc` (a shameless advertisement) to some of the heavy lifting for us, but the rest of the code remains simple.

This tutorial is somewhat inspired by the fantastic [Write Yourself a Scheme in 48 Hours](http://en.wikibooks.org/wiki/Write_Yourself_a_Scheme_in_48_Hours). I wanted to show that this was possible in other languages too. And like Haskell, many people are interested in learning C but have nowhere to start - well here is your excuse. Follow along with this and I can ensure you'll at least get a cool new toy at the end - and hopefully knowledge of a new language.

This type of tutorial is hard for the beginner - much harder than reading a book - but the advantage is that it teaches a number of implicit techniques and idioms that only a _real world_ project can capture.

Some of you might be wondering what kind of Lisp we'll be building (or what Lisp is at all). Lisp is a family of programming languages characterized by the fact that all their computation is represented by lists. This sounds scarier than it is. Actually Lisp is very easy, distinctive, and powerful. As for the brand of Lisp - it is one I've essentially pulled out of my ass for the purposes of this tutorial, I've designed it for minimalism, simplicity and clarity, and I've become quite fond of it along the way.

I've tried to make the tutorial as easy as possible, and friendly to beginners. But C is a difficult language, and not all of the concepts we are using are going to be familiar. If you've never seen C, or programmed before, then it is probably worth brushing up on some basics (at least the syntax, and pointers) before rushing on ahead. If you are already an expert in C then feel free to skip ahead.

My advice to beginners is to read over the first few exercises of [Learn C the Hard Way](http://c.learncodethehardway.org/book/) until you are confident that you can read and understand the first couple of examples. Otherwise when you come into issues or get stuck, it is going to be a real uphill battle later to try and find out where you are going wrong.

As you are following along with this tutorial please don't just copy and paste the example code! Type it out yourself. I know it is tedious and boring, but using this method will you come to understand the reasoning behind the code. Things will automatically click as you follow it along character by character. You may have an intuition as to why it _looks_ right, or what _may_ be going on but this will not always translate to a real understanding unless you do it yourself!


The Basics
----------

Here is some brief information on how to write, compile and run your first C program. For detailed information on getting set to program in C you are again directed toward [Learn C the Hard Way](http://c.learncodethehardway.org/book/) which has very good and thorough instructions on how to get your environment ready.

Start by opening a text editor and inputting the following:

```c
#include <stdio.h>

int main(int argc, char** argv) {
  puts("Hello, world!");
  return 0;
}
```

There are three parts to this program. First we include a _header_ called `stdio.h`. This allows us to use _standard_ _input/output_ functions which come included with C. One of these is the `puts` function you see later on in the program.

Secondly we create a function called `main`, returning an `int`, and taking as input an `int` called `argc` and a `char**` called `argv`. All C programs must contain this function and it acts as the entry point to the program. Don't worry about the inputs for now.

Finally we have `puts("Hello, world!")` (meaning _put string_), which just outputs `Hello, world!` to the terminal and `return 0;`, which exits the program saying there have been no errors.

Save this code somewhere and browse to it on the command line. Here you'll want to compile your code and output an actual _program_ we can run. Say your file is called `example.c`, you'll want to type the following. 

`cc -std=c99 -Wall example.c -o example`

This compiles the code in `example.c`, reporting any warnings, to the file `example`. If successful you should see a new file called `example` in the current directory. This can be called by typing `./example`. If all goes to plan you should see a friendly `Hello, world!` appear.

For later examples you'll need to include `mpc`. For this go and download the latest version of `mpc` from [http://github.com/orangeduck/mpc](http://github.com/orangeduck/mpc) and place the two source files `mpc.c` and `mpc.h` in the same directory as your code. You'll also have to update your compiler command to include `mpc` as follows:

`cc -std=c99 -Wall example.c mpc.c -lm -o example`

At the beginning of your code remember to add `#include "mpc.h"` which will give your program access to `mpc` functions.

### Tutorial Code

Here I'll link to the reference code for each section of the tutorial. It could be tempting to simply read the text and then copy and paste this code into your project and compile to see what it is like. Please don't do this. This code is for _reference_ and should only be used when good and stuck or if the text was unclear on what to do.

[example0.c]()


### Bonus Marks

Here are some things to try for bonus marks. These will be in all sections and will help if you are able to do them, but don't worry if you can't figure them out. Some are harder than others.

* Change the greeting to something else.
* Return a different number. What does this do?


An Interactive Prompt
---------------------

To start we want to build an interactive prompt. Using this will be the easiest way to test our program and see how it acts. This is called a _REPL_ and stands for _read_, _evaluate_, _print_ _loop_. It is a common way of interacting with a language. You may have used before in languages such as Python.

Before building a real _REPL_ we'll make a simple system that just echos any input right back to the user. We can later extend this to parse the user input and evaluate it as if it were an actual Lisp program.

The basic setup in C is simple. We want a loop that will prompt the user, and wait for any input. We declare a statically sized input buffer and use the `fgets` function to read into it. This function will read any input from the user up until a newline but wont overflow the buffer. We then print this input back to the user using `fprintf`. The user input from `fgets` includes the newline character so we don't need to add this to the end of `fprintf`.

```c
#include <stdio.h>

/* Declare a static buffer for user input of size 2048 */
static char input[2048];

int main(int argc, char** argv) {
   
  /* Print Version and Exit Information */
  fputs("Lispy Version 0.0.0.0.1\n", stdout);
  fputs("Press Ctrl+c to Exit\n\n", stdout);
   
  /* In a never ending loop */
  while (1) {
  
    /* Output our prompt */
    fputs("lispy> ", stdout);
    
    /* Read a line of user input of maximum size 2047 (save 1 char for null termination) */
    fgets(input, 2047, stdin);
    
    /* Echo input back to user */
    fprintf(stdout, "No You're a %s", input);
  }
  
  return 0;
}
```

There are a few new concepts here so lets go over them briefly. `static char input[2048];` declares a global array of 2048 characters. This is where we store the user input which is typed into the terminal. The `static` keyword is what makes it global. `fputs` and `fgets` are functions for putting and getting strings from the terminal. `stdout` and `stdin` are the names in C for terminal output and input. Finally `fprintf` is a way of printing messages in a kind of shorthand. In this case the `%s` is replaced by whatever string argument you pass in next - in our case `input`.

After compiling this you should have a little play around with it. You can use `Ctrl+c` to quit the program when you are done. If everything is correct you should get something that resembles the following:

```
$ example1
Lispy Version 0.0.0.0.1
Press Ctrl+c to Exit

lispy> hello
No You're a hello
lispy> my name is Dan
No You're a my name is Dan
lispy> Stop being so rude!
No You're a Stop being so rude!
lispy>
```

Now to create some kind of meaningful interaction!

### Tutorial Code

[example1.c]()

### Bonus Marks

* Change the prompt from `lispy> ` to something of your choice.
* Add an extra message to the _Version_ and _Exit_ Information
* Make the system echo something different back.


Introducing MPC
---------------

A programming language is much like a real language. There is some logic behind how it is structured, some rules as to what is a valid thing to say, and some way in which we can generate sentences.

A key observation in the understanding of language is that as well as rules, language contains repeated structures. The same is true of programming languages - an `if` statement body can contain any number of new statements, including another `if` statement itself (and so on). This means that although there might be an infinite number of different things that can be said in a language, we can still recognise and understand all of them with a finite number of rules and processes.

These rules are called a _grammar_ and can be written down using special notation. If we want to read, understand, and evaluate Lisp we need to write a _grammar_ for it first. Using the rules in a grammar we can try to write a program that _reads_ some sentence and builds an internal representation of it. Once we have this internal representation we can _evaluate_ it and perform the commands encoded by it. This is where `mpc` comes in.

`mpc` is a _Parser Combinator_ library. This is a library that allows you to build parsers. These are programs that understand and process languages. The key thing about a _Parser Combinator_ library is that it lets you build a parser just by specifying the grammar...sort of.

Many actually work by letting you write code that simply _looks_ a bit like a grammar. `mpc` does both. As well as letting you write code that _looks_ like a grammar you can actually write a grammar itself! For our purposes we are going to make use of the second option, but going over the first should be helpful too.

So what does code that _looks like_ a grammar.._look like_?

```c
#include "mpc.h"

int main(int argc, char** argv) {
  
  /* Requires some input on the command line */
  if (argc != 2) {
    puts("Usage: parser <flatmate>");
    return 0;
  }
  
  /* Build a new parser 'Flatmate' */
  mpc_parser_t* Flatmate = mpc_or(4, 
    mpc_string("Dan"),
    mpc_string("Chess"),
    mpc_string("Adam"),
    mpc_string("Lewis")
  );
  
  /* Parse the result into result 'r' */
  mpc_result_t r;
  if (mpc_parse("<stdin>", argv[1], Flatmate, &r)) {
    /* On success report flatmate */
    printf("Got a flatmate %s!\n", (char*)r.output);
    free(r.output);
  } else {
    /* On Failure report nothing */
    printf("Not a flatmate!\n");
    mpc_err_delete(r.error);
  }
  
  mpc_delete(Flatmate);

  return 0;
}
```

This code parses an input string and checks it is _either_ `Dan`, `Chess`, `Adam` _or_ `Lewis`. If it is, then it prints out the parsed string, otherwise it returns an error. If you squint your eyes you could attempt to read the above code like it is the definition of a language, _a grammar_ - "To Parse a 'Flatmate', parse (the string 'Dan') or (the string 'Chess') or (the string 'Adam') or (the string 'Lewis')".

Here is a more complicated example builting on the first.

```c
mpc_parser_t* Greet = mpc_also(
  mpc_string("Hello "), Flatmate, 
  free, mpcf_strfold); 

mpc_parser_t* Greetings = mpc_many(Greet, mpcf_strfold);
```

In this example `Greet` parses the string `"Hello "` followed by any `Flatmate` (_either_ `Dan`, `Chess`, `Adam` _or_ `Lewis`). It then returns these strings concatenated together (using the function `mpcf_strfold`). The parser `Greetings` recognizes zero or more occurrences of `Greet`, for example `"Hello Dan!"` or `"Hello DanHello Chess"` or `"Hello LewisHello LewisHello Lewis"` or any other combination! Again we can squint our eyes and try to read this like a grammar - "To parse a 'Greet', parse the string 'Hello ' followed by a 'Flatmate', and join them using 'mpcf_strfold'. To Parse a 'Greetings' parse many of 'Greet' and join them using 'mpcf_strfold'".

Using many more constructs we can slowly build up a parser that parse more and more complex languages. The code we use _sort of_ looks like a grammar but is functional too. This isn't always an easy task but there are a whole set of helper functions that build on simple constructs to make frequent tasks easy. These are all documented on the [mpc repository](http://github.com/orangeduck/mpc).

But as I mentioned before, `mpc` actually lets you simply write the grammar directly. You don't have to worry about `mpcf_strfold` or leaning how to use a bunch of functions. This is what we are going to use for our Lisp. As an example of this method, to recreate the above example parser `Greetings` we would write something that looks like this.

```c
mpc_parser_t* Flatmate  = mpc_new("flatmate");
mpc_parser_t* Greet     = mpc_new("greet");
mpc_parser_t* Greetings = mpc_new("greetings");

mpca_lang(
  "                                                            \
    flatmate  : \"Chess\" | \"Dan\" | \"Adam\" | \"Lewis\";    \
    greet     : \"Hello\" <flatmate>;                          \
    greetings : <greet>*;                                      \
  ",
  Flatmate, Greet, Greetings
);

/* Do Some Parsing... */

mpc_cleanup(3, Flatmate, Greet, Greetings);
```

Already without having a good understanding of what is going on it should be clear how much more _readable_ the grammar is in this format. We barely have to squint our eyes any more. You should also notice the process is now in two steps. First we create and name several rules using `mpc_new` and then we define them using `mpca_lang`.

The first argument to `mpca_lang` is a long multi-line string. This is the fabled _grammar_ specification you've heard so much about. It consists of a number of _rules_. Each rule has the name of a parser on the left, a colon `:`, and on the right it's definition terminated with a semicolon `;`. A definition consists of what needs to be parsed for this rule. To specify a string that needs to be parsed put it in `"` marks (escaped using `\` because they are inside a C string themselves). To specify a number of options (like _or_) use the bar `|`. To specify zero or more use `*`, for one or more use `+`, and to reference another rule put it's name in inside `<>` like so.

Hey, did anyone notice that the above paragraph sort of sounded like when I was specifying a _grammar_ in text like we were doing before? That's because it was! `mpc` actually uses itself internally to parse the input you give it to `mpca_lang`. And it does it by specifying a _grammar_ very similar to what I described above in text. How neat is that.

Enough theory - lets give this `mpc` thing a whirl.


### Tutorial Code

[example2.c]()
[example2a.c]()
[example2b.c]()

### Bonus Marks

* What would the grammar be like for a decimal number?
* What might the grammar look like for a sentence in simple English?
* If you are familiar with JSON or XML, what might their grammars look like?


A Basic Maths Grammar
---------------------

A simple grammar to implement would be a maths subset of Lisp that resembles [Polish Notation](http://en.wikipedia.org/wiki/Polish_notation). In this format the operator always comes first, followed by the operands. For example `(+ 1 2 6)` or `(+ 6 (* 2 9))`.

So a grammar for this, we can describe it in text something like this. "An _expression_ is either a _number_, or, in parenthesis, an _operator_ followed by one or more _expressions_". Now we can break this down into each of the individual parts and write it formally.

A number is an optional minus sign, followed by one or more of the characters in the range 0-9.

`number : /-?[0-9]+/ ;`

You'll notice this is written between two forward slashes `/`, which means it is a _regular expression_ (another type of grammar). For now don't worry to much about the syntax as there are a lot to say about regular expressions that are beyond the scope of this tutorial. Basically the `?` means optional, the `+` means one or more of, and the `[0-9]` means in the range `0` to `9`. The next one is easy.

An operator is either '+', '-', '*' or '/'.

`operator : '+' | '-' | '*' | '/' ;`

An expression is either a _number_ or, in parenthesis, an _operator_ followed by one or more _expressions_.

`expr : <number> | '(' <operator> <expr>+ ')' ;`

Finally a, program is an expression followed by the end of the input.

`lispy : <expr> /$/ ;`

Putting it all together we get the following.

```c
/* Create Some Parsers */
mpc_parser_t* Number = mpc_new("number");
mpc_parser_t* Operator = mpc_new("operator");
mpc_parser_t* Expr = mpc_new("expr");
mpc_parser_t* Lispy = mpc_new("lispy");

/* Define them with the following Language */
mpca_lang(
  "                                                     \
    number   : /-?[0-9]+/ ;                             \
    operator : '+' | '-' | '*' | '/' ;                  \
    expr     : <number> | '(' <operator> <expr>+ ')' ;  \
    lispy    : <expr> /$/ ;                             \
  ",
  Number, Operator, Expr, Lispy);
  
/* Do some Parsing... */

/* Undefine and Delete our Parsers */
mpc_cleanup(4, Number, Operator, Expr, Lispy);
```

So we create our four parsers using `mpc_new` and then define them using `mpca_lang`. We should put the first part of this code right at the beginning of our program, before any message is printed, and we should put the `mpc_cleanup` command at the end of our program right before returning from `main`.

Next we need to edit our `while` loop so that rather than just annoyingly echoing user input back it actually attempts to parse the input using our maths grammar. So take out the line with `fprintf` on and replace it with this code that attempts to parse the user input using our polish maths grammar.

```
/* Attempt to Parse the Input */
mpc_result_t r;
if (mpc_parse("<stdin>", input, Lispy, &r)) {

  /* On Success Print the AST */
  mpc_ast_print(r.output);
  mpc_ast_delete(r.output);
  puts("\n");
  
} else {

  /* Otherwise Print the Error */
  mpc_err_print(r.error);
  mpc_err_delete(r.error);
  
}
```

Here we use the `mpc_parse` function with our parser `Lispy` on the `input`. We have to give a name for the source this input is coming from (we just call it `"<stdin>"` for now) and also we need to output the result to some variable. For this we declare a variable `r` of type `mpc_result_t` and then use the _address of_ operator `&` to get a pointer to it. We then give `mpc_parse` this pointer. A pointer is like a house number that `mpc_parse` can deliver to. Rather than giving our whole house to the `mpc_parse`, so that he can place a parcel on the mat at the front door, and give it back to us, we just give him our house number and ask him to send the result from where he is.

Don't worry too much for now if you don't understand, pointers are a famously hard part of C, so we'll cover them in more depth later.

`mpc_parse` returns true or false depending on if the result of the parse was successful. If it is successful we just print out the _ast_, or _abstract syntax tree_ it returns, otherwise we print out the error. Both the error and the _ast_ returned have been newly created by `mpc_parse`, and it is our responsibility to delete them using the appropriate functions.

To compile this program you'll need to make sure you include `mpc`. For this go to the `mpc` repo [http://github.com/orangeduck/mpc](http://github.com/orangeduck/mpc) and download `mpc.h` and `mpc.c`. Put these in the same directory as your source file. To the top of your source file add `#include "mpc.h"` to give you access to all the new mpc functions. Say your code is in a file called `example3.c` you can now attempt to compile this using:

`cc -std=c99 -Wall example3.c mpc.c -lm -o example3`

You should be able to run it in the same way as before. If successful it should look something like this.

```
Lispy Version 0.0.0.0.2
Press Ctrl+c to Exit

lispy> (+ 1 2)
root:
  expr:
    char: '('
    operator: '+'
    expr: '1'
    expr: '2'
    char: ')'
  regex:


lispy> (+ 5  (* 2 2) )
root:
  expr:
    char: '('
    operator: '+'
    expr: '5'
    expr:
      char: '('
      operator: '*'
      expr: '2'
      expr: '2'
      char: ')'
    char: ')'
  regex:


lispy> hello
<stdin>:0:0: error: expected one or more of one of '0123456789' or '(' at 'h'
lispy> (/ 1dog & cat)
<stdin>:0:4: error: expected ')' at 'd'
lispy> (- 5 -1)
root:
  expr:
    char: '('
    operator: '-'
    expr: '5'
    expr: '-1'
    char: ')'
  regex:


lispy>
```

### Tutorial Code

[example3.c]()

### Bonus Marks

* Make a new operator like `%` available to use.
* Make the operators infix (that is between two numbers rather than before).
* Make the parser recognize some form of numbers in written format (e.g `one`, `two`, `three` ...).


Doing Arithmetic
----------------

Now we can read input, but we are still unable to evaluate it. We need to somehow traverse the output _Abstract Sytnax Tree_ from the previous section and use the information to calculate a result. The basic idea isn't too hard. For each `expr` node we should check if it is a list or a number. If it is a single number we should return that. If it is a list then we should look at the `operator` node and use that to decide what to do with the rest of the expressions contained inside.

But first we need to know how to traverse and manipulate this _Abstract Syntax Tree_. If we peek inside `mpc.h` we can have a look at the definition of `mpc_ast_t`.

```c
typedef struct mpc_ast_t {
  char* tag;
  char* contents;
  int children_num;
  struct mpc_ast_t** children;
} mpc_ast_t;
``` 

This looks pretty confusing syntactically, but actually the concept behind it is very simple. It reads that inside every `mpc_ast_t` node we have string called a `tag`, a string called `contents`, some number of children, and a list of those child nodes.

NOTE: Interlude on pointers

Lets start to write some code that will traverse this tree. One thing we should notice from the previous chapter is that the root of the tree always has the same structure. It has two children, the first an `expr` and the second a `regex` with no contents. This `regex` symbolizes the end of the input and can be ignored. Therefore we know we can evaluate the root of the tree just by evaluating the first child of it.

```c
long evaluate_root(mpc_ast_t* t) {
  /* Root's first child is always the main expression */
  return evaluate_expr(t->children[0]);  
}
```

So now the question is how to we evaluate an `expr`. Again we can rely on a few observations from the previous chapter. First we can see that if the `expr` has no children it is always a number. In that case we can just convert the number right away and return it.

If the expression does have children on the other hand it is more complicated. First we need to look at it's second child (the first child is open parenthesis) and see which operator it is. Then we need to apply this operator to the remaining children. We can iterate over the remaining children by continuously applying the operator until we reach a node that isn't tagged as `expr`. In our case this node will always be the closing parenthesis which are tagged as `char`.

To represent numbers we'll use the C type `long` which means a _long_ _integer_. To test the tag values for the nodes we can use the `strcmp` function. This function takes as input two strings and if they are equal it returns `0`.

```c
long evaluate_expr(mpc_ast_t* t) {
  
  /* If no children we know it is a number. Otherwise it is a list */ 
  if (t->children_num == 0) { return atoi(t->contents); }
  
  /* The operator is always second element (open parenthesis is first) */
  char* op = t->children[1]->contents;
  
  /* We store first expression value in 'x' */
  long x = evaluate_expr(t->children[2]);
  
  /* Iterate the remaining expressions using our operator */
  int i = 3;
  while (strcmp(t->children[i]->tag, "expr") == 0) {
    x = evaluate_op(x, op, evaluate_expr(t->children[i]));
    i++;
  }
  
  return x;  
}
```

To apply this operator we need to check what the operator is and perform the correct arithmetic on the passed in evaluated expressions.

```c
/* Use operator string to see which operation to perform */
long evaluate_op(long x, char* op, long y) {
  if (strcmp(op, "+") == 0) { return x + y; }
  if (strcmp(op, "-") == 0) { return x - y; }
  if (strcmp(op, "*") == 0) { return x * y; }
  if (strcmp(op, "/") == 0) { return x / y; }
  return 0;
}
```

Finally we need to pass the parsed _Abstract Syntax Tree_ to our new evaluation function for the root. And instead of printing the _AST_ directly we now want to print the result of the evaluation. For this we use `fprintf` again with the specifier `%li` - which again means _long int_. We still need to remember to delete the output from the parser after we are done evaluating it.

```c
long result = evaluate_root(r.output);
fprintf(stdout, "%li\n", result);
mpc_ast_delete(r.output);
```

If all of this is successful you should be able to now do some basic maths with your new programming language!

```
Lispy Version 0.0.0.0.3
Press Ctrl+c to Exit

lispy> (+ 5 6)
11
lispy> (- (* 10 10) (+ 1 1 1))
97
lispy> (- (/ 10 2) 20)
-15
lispy>
```

### Tutorial Code

[example4.c]()

### Bonus Marks

* Add support for the `%` operator, which returns the remained of a division. E.G `(% 10 6)` gives `4`
* Add support for the `**` operator which raises a number to the power of another E.G `(** 4 2)` gives `16` (four squared).
* Add support for `min` and `max` function which return the smallest and biggest in of a list of numbers respectively E.G `(min 10 5 3)` gives `3`, `(max 1 20)` gives `20`.


Error Handling
--------------

Some of you may have noticed a problem with the previous chapter's code. Try entering this into the prompt and see what happens

```
$ example5
Lispy Version 0.0.0.0.2
Press Ctrl+c to Exit

lispy> (/ 10 0)
```

Ouch. The program crashed upon trying to divide by zero. Our program might be able to produce parse errors but it still has no functionality for reporting errors in the evaluation of expressions. We need to build in some kind of error handling functionality. This can be awkward in C. We can't use exceptions and need to ensure any memory allocated is cleaned up correctly - but if we start off on the right track, it will pay off later on when our system gets more complex.

My preferred method for error handling in this context is to make errors a possible result of evaluating an expression. So we can say that, in lispy, an expression will evaluate to _either_ a _long_, or an _error_. To do this we need to make a data structure that can act as either one thing or anything. There are several methods to do this in C but for simplicity sake we are going to use a struct. A struct in C is 
like a collection of named values called fields. These values can be accessed with the `.` operator, giving the name of the field. A struct is declared using the `struct` keyword followed by, in curly brackets, a list of type-name pairs separated by semicolons.

We can surround the declaration on one side with `typedef` and the other with some identifier to name it. 

In our situation we want to create a struct with a field for some error message, a field for some number value, and a field for some other value to differentiate between if it is a _number_ or an _error_. The whole declaration looks like this:

```c
/* Declare New lval Struct */
typedef struct {
  int type;
  long num;
  int err;
} lval;
```

For the `type` field we need to give a list of potential values it can take. For this we can use an enumeration. An enumeration is like a list of values as might be given to represent days of the week, or months of the year.

```c
/* Create Enumeration of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR };
```

We also want to declare another enumeration to represent what error the _error_ lval actually is there for. We have three error cases in our particular program. As well as division by zero our program should also error if it somehow encounters an unknown operator, or it is passed a number that is too large to represented internally using a `long`. 

```c
/* Create Enumeration of Possible Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
```

This can now be used with syntax similar to other native C types. To make it easier to use we want to declare two functions that construct `lval`'s of either an _error_ type or a _number_ type.

```c
/* Create a new number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}
```

See how first an `lval` called `v` is created. Then it's fields are assigned using the `.` operator. And finally it is returned from the functions. When we now use an `lval` in our functions, if we want to do different behaviour based upon the type of it, we have to compare it to our enumerated types above. There is a concise way to do this in C using the `switch` statement. This takes some value and compares it to other known values. When the values are equal it executes the code that follows.

As an example of this we want to build a function that can print an `lval` no matter what type it is. For this we can use the `switch` statement.

```c
void lval_print(lval v) {
  switch (v.type) {
    /* In the case the type is a number print it, then 'break' out of the switch. */
    case LVAL_NUM: printf("%li", v.num); break;
    
    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check What exact Type of error it is and print it */
      if (v.err == LERR_DIV_ZERO) { printf("Error: Division By Zero!"); }
      if (v.err == LERR_BAD_OP)   { printf("Error: Invalid Operator!"); }
      if (v.err == LERR_BAD_NUM)  { printf("Error: Invalid Number!"); }
    break;
  }
}
```

We can also declare a cheeky function that does the above but appends a newline.

```c
void lval_println(lval v) { lval_print(v); putchar('\n'); }
```

The final stage is to edit our evaluation functions so that they work on `lval`s instead of `long`s. We need to ensure each of the functions works correctly upon encountering an error `lval`. In most cases this means passing the error out to the higher level of the evaluation so it can finally be reported at the end. First we'll take a look at our operator evaluation function:

```c
lval eval_op(lval x, char* op, lval y) {
  
  /* If either value is an error return it */
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }
  
  /* Otherwise do maths on the number values */
  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
    /* If second operand is zero return error instead of result */
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  
  return lval_err(LERR_BAD_OP);
}
```

Notice that the type signature has changed to take `lval`s, and we no longer use `long`s anywhere.

The first thing we check is that we've not been passed in any `lval`s that are errorous. We can't operator on these so we need to return them right away if we do. Secondly we edit the division operator to return a _division by zero_ error if the second operator is zero. This uses the C ternary operator which works something like this: `<condition> ? <if_true> : <otherwise>`. If the condition is true it returns what follows the `?`, otherwise it returns what follows `:`.

Finally if the operator string matches none of our known operators we return a _bad operator_ error.

Now we need to apply similar treatment to our expression evaluation function.

```c
lval eval_expr(mpc_ast_t* t) {
  
  if (t->children_num == 0) {
    long x = strtol(t->contents, NULL, 10);
    /* Check if there is some error in conversion */
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }
  
  int i = 3;
  char* op = t->children[1]->contents;  
  lval x = eval_expr(t->children[2]);
  
  while (strcmp(t->children[i]->tag, "expr") == 0) {
    x = eval_op(x, op, eval_expr(t->children[i]));
    i++;
  }
  
  return x;  
}
```

This is largely similar, but with the type signatures changed - but we have also changed how we extract numbers from the _abstract syntax tree_. Now we use the `strtol` function which is considered better and more versatile than `atoi`. THe `strtol` function sets a variable `errno` if there is something wrong with the input number (such as it is too large). We can use this to check the number is valid, and if not return a _bad number_ error.

Finally the type signature of the root evaluation function needs changing.

```c
lval eval(mpc_ast_t* t) { return eval_expr(t->children[0]); }
```

And we need to change how we print the result found by it to use our newly defined printing function.

```c
lval result = eval(r.output);
lval_println(result);
mpc_ast_delete(r.output);
```

And we are done! It may have seemed likt this chapter has been a whole lot of fluff to solve a relatively minor issue, but we've actually built a lot of groundwork that we are going to use in the rest of the program for more advanced stuff. Although boring, error handling shouldn't be avoided! It isn't just the life of the user that it enhances. It can make your own life much easier when you come to debug your program.

### Tutorial Code

[example5.c]()

### Bonus Marks

* Find out how `union`s work in C and use them to define `lval`. Hint: It doesn't just mean changing `struct` to `union`.


Lists
-----

Some of you may have guessed that Lisp is a play on the word _list_, but so far we've not really seen much of the fabled beast. In this chapter we rectify that, by finally converting our program into something that could vaguely be called a Lisp. The first step is to change our program so that the input is represented internally using lists. The second step is to introduce a new kind of list we can use to store multiple values and much more. This chapter is important because by introducing lists we finally enter the world of Lisp - where thing differ by taste, and many people have different opinions on what is _right_.

Like a Highschool Physics teacher I may be about to teach you something that you will later find out to be _wrong_. But there is no need to panic. The reason I'm doing it is that it is a valid simplification that aids understanding. It is an abstraction. But it's important to mention that, because creating abstractions is not an objective task - there is some art to it. Like a physics professor with their own pet-theory of the universe, I've become pretty fond of mine, but that doesn't mean you will be.

In this tutorial you are encouraged to change things the way you want them. If you don't like the syntax then change it. If you want to call it something else then do. Think the functions should be named something else? Feel free. I think self-expression is the most important thing in programming so these are skills I want to teach. What I say for the rest of this tutorial is just a template. Keeping that in mind...

In our Lisp there are two types of list.

The first we are going to call S-Expressions. These are the lists of numbers, symbols, other S-Expressions and anything else, surrounded in parenthesis `()`. These are the guys we are used to so far. They are used to read in and store the actual structure of the program. Most importantly they have the particular evaluation behaviour typical of Lisps. That to evaluate an S-Expression we look at the first item in the list, and take this to be the operator. We then look at all the other items in the list and apply them to this first item to get the result.

The second type of list we are going to call Q-Expressions. Again these are lists of numbers, symbols, and other Expressions, but these we put them in curly brackets `{}`. Q-Expressions are used to store lists of things we don't want to be evaluated. This could be numbers, functions, S-Expressions or anything else. When encountered by the evaluation function Q-Expressions are _not_ evaluated like S-Expressions - they are left exactly as they are. This unique behaviour makes them ideal for a whole number of purposes we will encounter later.

Just using these two types of list, a handful of operators, and a bit of creativity, we can build a programming language that is insanely powerful and flexible.

In C no concept of list can be explored without dealing properly with pointers. Pointers are a famously misunderstood aspect of C. They are difficult to teach because while being conceptually very simple, they come with a lot of new terminology, and often no clear use-case. This makes them appear far more monstrous than they are. Luckily for us, we have two ideal use-cases, we need to use them for right away. Both of these use cases are extremely typical in C, and will likely end up being how you use pointers 90% of the time.

When you call a function in C arguments are always passed _by value_. This means a copy of them is passed to the function, which it can do what it wants. This is true for `int`, `long`, `char` and `struct`s such as `lval`. Most of the time this is great but occasionally it can cause issues. For example if we have a large struct containing many other structs suddenly the amount of data that needs to be copied around can become huge. This data that is copied around must always be a fixed size. Otherwise the compiler has no way of generating valid code. This introduces a second problem. We have no way of creating a data structure which has a varying size, such as a list.

To get around these issues the developers of C came up with a clever idea. They observed that inside a computer each piece of data exists at some known location. More specifically one can visualize memory as a huge one dimensional array of boxes, each containing a single byte of data. Rather than copying the data itself, one can instead copy a number representing the location of where this data starts. This is known as _the address_ of the data. 

By using addresses instead of the data itself one can allow a function to access and modify data without making a copy of it.




### Tutorial Code

[example6.c]()


### Bonus Marks



Variables
---------

Functions
---------

Conditionals
------------


Input/Output & Beyond
---------------------



Conclusion
----------




