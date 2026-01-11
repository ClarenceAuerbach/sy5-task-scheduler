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

To remove the executables:
```bash
make distclean
```

## Usage
Tadmor **will** provide the following options:

---

### Task creation/deletion

```
-c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] CMD [ARG_1] ... [ARG_N]
```

: create a simple task

```
-s [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] TASKID_1 ... TASKID_N
```

: sequential combination of the tasks identified by `TASKID_1` to `TASKID_N`;
the command to execute is `(CMD_1 ; ... ; CMD_N)`

```
-n
```

: used in combination with the previous options, defines a task **without an execution schedule** (i.e., it will never be executed on its own, and is intended to be combined with other tasks to form a complex command)

```
-r TASKID
```

: delete the task identified by `TASKID`

---

### Server data consultation

```
-l
```

: list all tasks

```
-x TASKID
```

: display the dated list of return values from the executions of the task identified by `TASKID`

```
-o TASKID
```

: show the standard output of the last completed execution of the task identified by `TASKID`

```
-e TASKID
```

: show the standard error output of the last completed execution of the task identified by `TASKID`

---

### Miscellaneous

```
-p PIPES_DIR
```

: specify the directory containing the communication pipes with the daemon
(default: `/tmp/$USER/erraid/pipes`)

```
-q
```

: stop the daemon

## Roadmap
Git repository exists

AUTHORS.md file is present

Successful compilation at the repository root using make

Interpretation of a provided directory structure defining the tasks to execute

Execution of simple tasks at scheduled times

Execution of task sequences at scheduled times

Log file updates (return values and standard outputs)

Beginning of tadmor, implementations of requests and tubes

Added strings and reorganised the project

Communication between tadmor and erraid. Assured by erraid_req

Implementation of multiple type of tasks



## Authors and acknowledgment
Thanks to all contributors.

## License
No license defined yet.
