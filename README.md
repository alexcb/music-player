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
 libmicrohttpd git version 4adf1c6d1744e1ac4cb0c88817a3726c3038b919
 libwiringPi (see http://wiringpi.com/download-and-install/)

libao-dev libmpg123-dev libjson-c-dev libmicrohttpd-dev

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



HTTP_PORT=8080 WEB_ROOT=./resources/web/ ./my123 '/home/alex/Downloads/alex/Soulseek Downloads/complete' streams foo



# To load a new playlist
POST /playlists/load
{name: "foo", playlist: ["path/a", "path/b", ...]

# To play a playlist
curl -s -XPOST 'raspberrypi.local/playlists?name=foo&track=0'
