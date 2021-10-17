/* 
 * tsh - A tiny shell program with job control
 * 
 * Name: Meet Pravinbhai Sinojia
 * ID: 201601126
 * Email: 201601126@daiict.ac.in
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];  //String array to store arguments provided in command line

    /*
     * Parseline function is used to break command-line string into different arguments and storing them in argv[] array.
     * It also checks if last character is '&'. If so, it returns 1 to indicate that the process is to be run in background.
     */
    int isBG = parseline(cmdline,argv);
    

    if(argv[0]==NULL){ //If user just press ENTER without typing any command, do nothing and give prompt back
        return;
    }

    sigset_t sSet;
    sigemptyset(&sSet);             //creating an empty sigset
    sigaddset(&sSet, SIGCHLD);      //adding SIGCHLD in the set

    pid_t cpid;
    /*
     * If user entered a built-in command, then function builtin_cmd() executes the command and returns true.
     * In that case, condition inside if evaluates to be false. 
     * Otherwise, the if condition evaluates to true and command is run using fork() and exec(). 
     */
    if(!builtin_cmd(argv)){
        sigprocmask(SIG_BLOCK, &sSet, NULL); //Block SIGCHLD while parent forks to avoid race-condition
        if((cpid=fork())==0){
            setpgid(0,0); //Create a new process group with child as leader
            if(execvp(argv[0],argv)<0){ //exec to run command in newly created process
                printf("%s: Command not found\n",argv[0]); //Give error if exec fails (due to bad command) and terminate child process
                exit(0);
            }
        }

        if(isBG){   //For backgroung process
            addjob(jobs,cpid,BG,cmdline); //Add job to the job-table
            sigprocmask(SIG_UNBLOCK, &sSet, NULL); //Unblock SIGCHLD
            printf("[%d] (%d) %s",pid2jid(cpid),cpid,cmdline); //Print process-ID and job-ID of the job created
        }

        else{      //for foreground process
            addjob(jobs,cpid,FG,cmdline); //Add job to the job-table
            sigprocmask(SIG_UNBLOCK, &sSet, NULL); //Unblock SIGCHLD
            waitfg(cpid); //Wait for job to finish, and then give prompt back to the user
        }
    }

    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command, then execute it using this function.  
 */
int builtin_cmd(char **argv) 
{
    /*quit command*/
    if(strcmp(argv[0],"quit")==0){
        int i;
	    for(i=0;i<MAXJOBS;i++){ //If there are any jobs in ST (Stopped) state, then give error and do not quit
            if(jobs[i].state==ST){    
                printf("There are some processes in stopped state so can't quit\n");
                return 1;
            }
        }
        exit(0); //If not job in ST state, then quit.
    }

    /*jobs command*/
    else if(strcmp(argv[0],"jobs")==0){
	    listjobs(jobs); //list all jobs present in job-table using helper listjobs() function
	    return 1;
    }

    /*bg or fg commands*/
    else if(strcmp(argv[0],"fg")==0 || strcmp(argv[0],"bg")==0){
	    do_bgfg(argv); //Put specified job in background or foreground with helper dp_bgfg() function
	    return 1;
    }
    return 0;  /* return 0 if it is not a built-in command, so eval function will take care of it. */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    /*First check for invalid (or missing) arguments*/

    if(argv[1]==NULL){ //if no argument provided
        printf("fd command requires PID or %cjobid argument\n",'%');
        return;
    }

    int L=strlen(argv[1]); //stores length of argument provided to fg or bg command

    if(argv[1][0]=='%'){ //if first character in argv[1] is '%', it means job-ID must be provided
        for(int i=1;i<L;i++){ 
            if(!(argv[1][i]>='0' && argv[1][i]<='9')){ //if any of next character is not a digit, then it cannot be a valid JID.
                printf("fg: argument must be a PID or %cjobid\n",'%');
                return;
            }
        }
    }

    else{
        for(int i=0;i<L;i++){ //if first character in argv[1] is not '%', then it must be a PID
            if(!(argv[1][i]>='0' && argv[1][i]<='9')){
                printf("fg: argument must be a PID or %cjobid\n",'%'); //if any character is not a digit, it cannot be a valid PID
                return;
            }
        }
    }

    int num;    //we will store the PID or JID provided as argument here

    struct job_t *jobsPtr;      //this will point to job entry in job-table if provided PID or JID is valid


    /*Next part is to extract PID or JID from argument provided*/

    if(argv[1][0]=='%'){ //if it is JID, skip first character and store remaining in a string
        char tem[L-1]; //to store JID in string format

        int i;
        for(i=1;i<L;i++){
        tem[i-1]=argv[1][i];
        }
        
        num = atoi(tem); //obtain JID in number form from string form
    }

    
    else{ //if it is PID, argv[1] already contains PID in string form
        num = atoi(argv[1]); //convert it to numerical form
    }


    /*Next part is to get job entry corresponding to given PID or JID*/

    if(argv[1][0]=='%'){
        jobsPtr=getjobjid(jobs,num);    //search job-table by JID
    }

    else{
        jobsPtr=getjobpid(jobs,num);    //search job-table by PID
    }

    /* If given JID/PID does not exist in job-table, then give an error */
    if(jobsPtr==NULL){
        if(argv[1][0]=='%'){
            printf("%s: No such job\n",argv[1]);
            return;
        }
        else{
            printf("(%d): No such process\n",num);
            return;
        }
    }


    /* If the job is in ST (Stopped) state */

    if(jobsPtr->state == ST){
        //if the command is "bg", then resume it in background
        if(strcmp(argv[0],"bg")==0){
            jobsPtr->state = BG;    //changing state from ST to BG
            printf("[%d] (%d) %s",jobsPtr->jid,jobsPtr->pid,jobsPtr->cmdline); //print information about job resumed
            kill(-num,SIGCONT);     //sends the SIGCONT signal to the process group of that job to resume it
        }

        //if the command is "fg", then resume the job in foreground
        if(strcmp(argv[0],"fg")==0){
            jobsPtr->state = FG;    //changing state from ST to FG
            kill(-num,SIGCONT);     //sends the SIGCONT signal to the process group of that job to resume it
            waitfg(jobsPtr->pid);   //wait for the process to terminate, since it is foreground job
        }
    }


    /* If the job is not in stop state and the command is "bg", then nothing needs to be done, just print information about it */

    else if(strcmp(argv[0],"bg")==0){
        printf("[%d] (%d) %s",jobsPtr->jid,jobsPtr->pid,jobsPtr->cmdline);
    }


    /* If the command is "fg" and the process is running in background, then move it to foreground */

    else if(jobsPtr->state == BG && strcmp(argv[0],"fg")==0){
        jobsPtr->state = FG;    //change the state from BG to FG
        waitfg(jobsPtr->pid);   //wait for the process to terminate
    }

    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    struct job_t *jobsPtr;
    jobsPtr = getjobpid(jobs,pid);  //to get the job entry corresponding to given PID

    while(jobsPtr!=NULL && jobsPtr->state == FG){   //If such a job exists and running in foreground
        sleep(1);  //Sleep for 1 second, then check if it is still running in foreground. If so, wait another second and so on.
    }

    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int status=-1;
    pid_t pid;
    struct job_t *jobsPtr;
    //Checking for any process which is terminated
    while((pid = waitpid(-1,&status,WNOHANG|WUNTRACED))>0){
        jobsPtr = getjobpid(jobs,pid);  //geting the job containing that pid

        //if it exited (Terminated normally), then delete the corresponding job-table entry.
        if(WIFEXITED(status)){
            deletejob(jobs,pid);  
        }

        //If it terminated due to signal, then print which signal terminated it and then remove its job-table entry.
        if(WIFSIGNALED(status)){
            printf("Job [%d] (%d) terminated by signal %d\n",jobsPtr->jid,jobsPtr->pid,WTERMSIG(status));
            deletejob(jobs,pid);    
        }

        //If is stopped by the signal, then print which signal stopped it and change its state to ST, do not delete it.
        if(WIFSTOPPED(status)){
            printf("Job [%d] (%d) stopped by signal %d\n",jobsPtr->jid,jobsPtr->pid,WSTOPSIG(status));
            jobsPtr->state = ST;
        }
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t fjob = fgpid(jobs);

    //Check if any job is running in forground
    if(fjob!=0){
        kill(-fjob,SIGINT); //If so then send SIGINT signal to the process group of that job
    }

    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t fjob = fgpid(jobs);

    //Check if any job is running in forground
    if(fjob!=0){
        kill(-fjob,SIGTSTP); //If so then send SIGTSTP signal to the process group of that job
    }

    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



