
# FasterSameFrames
This small project goes back to late 2017/early 2018 when i wanted to create a program in C++ to fast-forwarding videos that might have a lot of same/similar frames in them (coding session videos or painting timelapses for example) because i couldn't figure out a method back then.
Now, in 2020, I decided to take a look at it with the main goal to implement multi-threading, I'll also take a look from time-to-time to make it *slightly* better.

### Building
```sh
git clone https://github.com/CosminPerRam/FasterSameFrames.git
cd FasterSameFrames
mkdir bin
make
./bin/main <args>
```

### Usage
The usage can be shown by running:
```sh
./main --help
```

### Warning
Currently, multithreading is not working, working on a fix.
