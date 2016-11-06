# mpg123hack
hacking at mpg123

gcc -std=gnu99 *.c -lcurl -lao -lmpg123

./a.out file

or 

./a.out http://stream

dependencies:
 libao-dev
 libmpg123-dev
 libwiringPi (see http://wiringpi.com/download-and-install/)

todo:
 remove wiringPi as a dependency; just read/write to the memory addresses instead maybe? or find a different lib that's
 more lightweight and doesn't shell out to setup pins


config settings:
  sudo amixer cset numid=3 2
