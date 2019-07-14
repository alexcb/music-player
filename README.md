# mpg123hack
hacking at mpg123

gcc -std=gnu99 *.c -lcurl -lao -lmpg123

./a.out file

or 

./a.out http://stream

alex@raspberrypi:~/mpg123hack $ LOG_LEVEL=DEBUG WEB_ROOT=/home/alex/mpg123hack/resources/web/ sudo -E ./my123 /media/nugget_share/music/alex-beet/ streams playlists /home/alex/id3_cache


dependencies:
 libao-dev
 libmpg123-dev
 libssl-dev
 libmicrohttpd git version 4adf1c6d1744e1ac4cb0c88817a3726c3038b919
 libwiringPi (see http://wiringpi.com/download-and-install/)
 libasound2-dev

libao-dev libmpg123-dev libjson-c-dev libmicrohttpd-dev libttspico-dev

todo:
 remove wiringPi as a dependency; just read/write to the memory addresses instead maybe? or find a different lib that's
 more lightweight and doesn't shell out to setup pins


config settings:
  # set audio to analog out
  sudo amixer cset numid=3 2
  # set volume to 100%
  sudo amixer cset numid=1 0

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

libao
reads from /etc/libao.conf

configured to use alsa

# list all drivers
sudo aplay -l

raspberrypi uses alsa; desktop uses pulse
 
alex@raspberrypi:~ $ sudo amixer
Simple mixer control 'PCM',0
  Capabilities: pvolume pvolume-joined pswitch pswitch-joined
  Playback channels: Mono
  Limits: Playback -10239 - 400
  Mono: Playback 400 [100%] [4.00dB] [on]

# next time it locks up, see if aplay still works:
sudo aplay Left-Right\ Channel\ Audio\ Test.mp3

notes from 2019-jan
sudo amixer cset numid=3 1





# setting up on a new pi:
sudo dd bs=4M if=/home/alex/Downloads/2018-11-13-raspbian-stretch-lite.img of=/dev/mmcblk0 conv=fsync

# Then remove, and re-insert SD card, to have ubuntu auto-mount it.

# configure the wifi
create /media/alex/boot/wpa_supplicant.conf with:
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
network={
    ssid="YOUR_NETWORK_NAME"
    psk="YOUR_PASSWORD"
    key_mgmt=WPA-PSK
}

# enable ssh by default by creating an empty file:
touch /media/alex/boot/ssh

# setup a static IP
edit /media/alex/rootfs/etc/dhcpcd.conf, and include:

interface wlan0
static ip_address=192.168.0.11/24
static routers=192.168.0.1
static domain_name_servers=8.8.8.8

# sync and unmount
sudo sync
sudo umount /media/alex/rootfs 
sudo umount /media/alex/boot 


# connect via ssh using user: pi, password: raspberry
ssh pi@192.168.0.11

# create a user:
sudo -i
useradd alex
mkdir /home/alex
chown alex:alex /home/alex/
usermod -s /bin/bash alex
echo 'alex ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
passwd alex

#logout
ssh-copy-id alex@192.168.0.11
scp  .ssh/id_rsa* 192.168.0.11:.ssh/.

ssh 192.168.0.11
echo $UID # and make note that it is 1000 (or update the below fstab command)

# setup nas
mkdir -p /media/nugget_share/music
echo '//192.168.0.3/music /media/nugget_share/music cifs uid=1000,username=alex,password=mypasswordgoeshere,iocharset=utf8,ro,nounix,file_mode=0777,dir_mode=0777,sec=ntlm,vers=1.0 0 0' >> /etc/fstab
mount -a

# install tools
apt update
apt upgrade
apt install git wiringpi libasound2-dev libmpg123-dev libjson-c-dev libssl-dev

# install special version of libmicrohttpd
# ( the libmicrohttpd-dev version has MHD_create_response_for_upgrade wrapped by an #if 0 guard ) 
libmicrohttpd:
wget ftp://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.63.tar.gz
tar xzf libmicrohttpd-0.9.63.tar.gz
cd libmicrohttpd-0.9.63
./configure
make
make install
ldconfig

cd
git clone git@github.com:alexcb/music-player.git
cd music-player
make
touch streams
sudo cp init.d-music /etc/init.d/music

scp 192.168.0.10:id3_cache 192.168.0.11:id3_cache
