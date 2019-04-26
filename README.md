# Magic-Conch-Shell
This is a shell application similar to Bash that supports various builtin commands, as well as multi-layered pipes.

You can use it like any shell, but keep in mind that my shell does not support autocomplete or similar functions.
My program dynamically allocates memory for the user input and subsequent arrays of string arrays to easily store
and give access to the machine-readable user input.

The char*** called "parsed_input" works as follows:

Say you input this command -> "ls -als | wc"

Then parsed_input's first layer/index(char** parsed_input[0]) contains the string array for "ls -als".

Breaking it down another layer(char* parsed_input[0][0]) contains the first string of the first piped command, which is "ls".

The last/bottom layer(char parsed_input[0][0][0]) is the first char of the first string in the first piped command, which is 'l'.


Efficiently allocating memory for this was a real pain, but there are no memory leaks! Totally worth it!
