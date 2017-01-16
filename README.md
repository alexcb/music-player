# mpg123hack
hacking at mpg123

gcc -std=gnu99 *.c -lcurl -lao -lmpg123

./a.out file

or 

./a.out http://stream

dependencies:
 libao-dev
 libmpg123-dev
 libssl-dev
 libmicrohttpd
 libwiringPi (see http://wiringpi.com/download-and-install/)

todo:
 remove wiringPi as a dependency; just read/write to the memory addresses instead maybe? or find a different lib that's
 more lightweight and doesn't shell out to setup pins


config settings:
  # set audio to analog out
  sudo amixer cset numid=3 2
  # set volume to 100%
  sudo amixer cset numid=1 400

Notes about amixer, one can call `sudo amixer cget numid=<any ID here>` to get a description and current value of audio settings



watch -n 0.2 '(git status | grep modified) && gitfixupandpush'




libmicrohttpd:
wget ftp://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.52.tar.gz
tar xzf libmicrohttpd-0.9.52.tar.gz
cd libmicrohttpd-0.9.52
./configure
make
make install
