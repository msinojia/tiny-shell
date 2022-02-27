# Tiny Shell (*tsh*)
A simple Unix shell program that supports job control. The purpose of this project was to become more familiar with the concepts of process control and signaling.

## General Overview
A shell is an interactive command-line interpreter that runs programs on behalf of the user. A shell repeatedly prints a prompt, waits for a command line on stdin, and then carries out some action, as directed by the contents of the command line.

Unix shells support the notion of job control, which allows users to move jobs back and forth between background and foreground, and to change the process state (running, stopped, or terminated) of the processes in a job. Typing ctrl-c causes a SIGINT signal to be delivered to each process in the foreground job. The default action for SIGINT is to terminate the process. Similarly, typing ctrl-z causes a SIGTSTP signal to be delivered to Shell project Page 3 each process in the foreground job. The default action for SIGTSTP is to place a process in the stopped state, where it remains until it is awakened by the receipt of a SIGCONT signal. 

Unix shells also provide various built-in commands that support job control. For example:
- *jobs*: List the running and stopped background jobs. 
- *bg \<job\>*: Change a stopped background job to a running background job. 
- *fg \<job\>*: Change a stopped or running background job to a running foreground job. 
- *kill \<job\>*: Terminate a job

## Specification

The *tsh* shell have the following features:

- The prompt is the string “*tsh>*”.
- The command line typed by the user should consist of a *name* and zero or more arguments, all separated by one or more spaces. If *name* is a built-in command, then *tsh* handles it immediately and wait for the next command line. Otherwise, *tsh* assumes that name is the path of an executable file, which it loads and runs in the context of an initial child process (In this context, the term *job* refers to this initial child process).
- *tsh* does not support pipes (|) or I/O redirection (< and >).
- Typing *ctrl-c* (*ctrl-z*) causes a SIGINT (SIGTSTP) signal to be sent to the current foreground job, as well as any descendents of that job (e.g., any child processes that it forked). If there is no foreground job, then the signal has no effect.
- If the command line ends with an ampersand, then *tsh* runs the job in the background. Otherwise, it runs the job in the foreground.
- Each job can be identified by either a process ID (PID) or a job ID (JID), which is a small positive integer assigned by *tsh*. JIDs can be denoted on the command line by the prefix “*%*”. For example, “ *%5*” denotes JID 5, and “*5*” denotes PID 5.
- *tsh* supports the following built-in commands:
  - The *quit* command terminates the shell.
  - The *jobs* command lists all background jobs.
  - The *bg \<job\>* command restarts *\<job\>* by sending it a SIGCONT signal, and then runs it in the background. The *\<job\>* argument can be either a PID or a JID.
  - The *fg \<job\>* command restarts *\<job\>* by sending it a SIGCONT signal, and then runs it in the foreground.
  - *tsh* reaps all of its zombie children. If any job terminates because it receives a signal that it didn’t catch, then *tsh* recognizes this event and prints a message with the job’s PID and a description of the offending signal.

## How to run
Clone this repository and run *tsh.c* file. It will start the shell and give a prompt. Run any valid command as described in the *Specification* above in order to start a new job or manage the existing ones. Run *quit* to stop the shell.
