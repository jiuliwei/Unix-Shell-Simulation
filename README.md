# Unix-Shell-Simulation

## Compiling:
You should load the **gcc 9.2.0** with the following command:

`module load gcc-9.2`

## How to run it:

Run `make` to generate an executable file named **nyush** in the current working directory. Then run `./nyush`.

## The shell is essentially a command-line interpreter. It works as follows:
- It prompts you to enter a command.
- It interprets the command you entered.
    - If you entered a built-in command (e.g., `cd`, `jobs`, `fg`, and `exit`), then the shell runs that command.
    - If you entered an external program (e.g., `/bin/ls`), or multiple programs connected through pipes (e.g., `ls -l | less`), then the shell creates child processes, executes these programs, and waits for all these processes to either terminate or be suspended.
    - It can handle the I/O redirection, such as `cat > output.txt` or `cat < input.txt`
    - If you entered something wrong, then the shell prints an error message.
- Rinse and repeat until you enter the built-in command `exit`, at which point it exits.
    - It has signal handling module to ignore termination signals, such as **SIGINT**, **SIGQUIT**, **SIGTERM** and **SIGTSTP**.