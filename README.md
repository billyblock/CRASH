# Custom Linux Shell

## Overview

CRASH (Crashes Randomly and Sometimes Hangs) is a custom Linux Shell written in **C++** that has many of the same behaviors as BASH.

The shell can execute commands, manage background processes, keep command history, run startup scripts, and even suggests the closest valid command when a command is not found.

## Features

- Interactive Linux Shell prompt with current user and current working directory.
- Executes programs using PATH lookup and recursive file searching.
- Command suggestion using Levenshtein Distance.
- Supports:
    - Background processes with: ```&```
    - Sequential commands with: ```;```
    - Environment Variable expansion and assignment.
    - Change Directory with: ```cd```
    - Script execution with: ```.(filename)``` 
- Full implementation of GNU Readline.
- Startup config with .crash script.
- Signal handling to terminate processes.


## Example

A Project like this is best experienced when treated like a sandbox. Try it out for yourself!

```
CRASH-user /home/user$ mkdid

Command not found
Did you mean: mkdir?
```

## Running the Program

You will need G++ and the GNU Readline Library installed. This project only works on **Linux**. If you are on windows I would recommend using WSL or any VM of your choosing.

1. Install GNU Readline Library.
    - ```sudo apt install g++ libreadline-dev```

2. Compile it using the GNU Readline Library.
    - ```g++ crash.cpp -lreadline -o crash```

3. Run the shell with:
    - ```./crash```

Optional startup files:
- ```.crash``` will execute commands on start.
- ```.crashHistory``` will keep a history of all commands.

## Why I built this?

This was my final project for my Operating Systems class. It deepened my understand of operating systems and low-level programming.

CRASH is one of my favorite projects that I have ever created!
