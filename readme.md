smallsh assignment

How to compile this program:
gcc -std=gnu99 shell.c token.c input-handling.c -o smallsh

What is currently working:
- 1. The command prompt
- 2. Comments & Blank Lines
- 4. Built-in Commands
- 5. Executing Other Commands
- 7. Foreground and background commands

What is partially working, but needs fixes:
- 3. Expansion of Variable $$
	(Issues with memory allocation result in certain expansions
	 "leaking into" the next token, causing unwanted modifications)
- 8. Signals SIGINT & SIGTSTP
	(These actually seem to work for the most part, but my current
	 implementation is extremely crude, and some signal handlers
	 are taking actions they shouldnt be doing.)

