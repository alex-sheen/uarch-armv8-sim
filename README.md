# ARMv8 Simulator | CMSC 22200
UChicago CMSC-22200 Computer Architecture, Aut 2021

In this course, I wrote a C program that is an instruction-level simulator for a subset of the ARMv8 instruction set. 

## Features
### 5-Stage Pipeline
* handles branch cases, stall cases, and bypass cases

### Branch Prediction
* uses an 8-bit global branch history register (GHR) and a branch target buffer (BTB)

### Data Cache and Memory Cache
* LRU, write-through, allocate-on-write


## Getting Started
These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. Have fun!

### Run
1. Clone this repo into your folder of choice.
    ```
    $ git clone https://github.com/alex-sheen/uarch-armv8-sim.git
    ```
[TODO...]
