# SY5-Task-Scheduler

## Description
A daemon-client pair allowing a user to automate the **periodic execution of tasks** at specified times, similar to the [cron](https://en.wikipedia.org/wiki/Cron) utility.

## Installation

Clone the repository:

```bash
git clone https://moule.informatique.univ-paris-diderot.fr/rialland/sy5-task-scheduler.git
```
If you don’t already have it, install [GCC](https://gcc.gnu.org/) and then compile the project:
```bash
make
```
First run the deamon process with :
```bash
./erraid [-F] [-R RUN_DIR] [-P PIPES_DIR]
```
Then use tadmor freely :
```bash
./tadmor [-p PATH] [-l|-q|-r TASKID|-x TASKID|-o TASKID|-e TASKID|-c [-m minutes][-h hours][-d days] cmd arg1...argn]
```

To remove the executables:
```bash
make distclean
```

## Usage
Tadmor provides the following options:

## Task creation

- `-c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] CMD [ARG_1] ... [ARG_N]`  
  Creation of a simple task

- `-s [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N`  
  Sequential combination of the tasks with identifiers `TASKID_1` to `TASKID_N`;  
  the command to be executed is `( CMD_1 ; ... ; CMD_N )`

- `-p [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N`  
  Pipeline combination of the tasks with identifiers `TASKID_1` to `TASKID_N`;  
  the command to be executed is `( CMD_1 | ... | CMD_N )`

- `-i [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 TASKID_2 [TASKID_3]`  
  Conditional combination of the tasks with identifiers `TASKID_1`, `TASKID_2`,  
  and optionally `TASKID_3`;  
  the command to be executed is  
  `( if CMD_1 ; then CMD_2 ; else CMD_3 ; fi )`

- `-n`  
  Used in combination with the previous options, defines a task  
  without an execution schedule (therefore it will never be executed),  
  and is intended to be combined with other tasks to create a complex command

## Task deletion

- `-r TASKID`  
  Deletion of the task with identifier `TASKID`

---

# Server Data Consultation

- `-l`  
  List of tasks with their identifiers

- `-x TASKID`  
  Dated list of return values from the executions of the task  
  with identifier `TASKID`

- `-o TASKID`  
  Standard output of the last complete execution of the task  
  with identifier `TASKID`

- `-e TASKID`  
  Standard error output of the last complete execution of the task  
  with identifier `TASKID`

---

# Miscellaneous

- `-P PIPES_DIR`  
  Defines the directory containing the communication pipes with the daemon  
  (default: `/tmp/$USER/erraid/pipes`)

- `-q`  
  Stop the daemon

## Roadmap

1st milestone:
Task array initialization 
Task task executions
Next execution times calculation 

2nd milestone:
Erraid correction for tests
Tube implementation
Request and response for consultation

3rd milestone:
Redisigning architecture for cleaner and safer code  
Client request writing correction for tests
Server response correction for tests
Create and remove options implementation

End of the Project !

## Authors and acknowledgment
Thanks to all contributors.

## License
No license defined yet.
