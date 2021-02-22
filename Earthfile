deps:
    FROM ubuntu:latest
    ENV DEBIAN_FRONTEND=noninteractive
    RUN apt-get update && apt-get install -y \
        build-essential \
        libmpg123-dev \
        libpulse-dev \
        curl \
        libjson-c-dev \
        libssl-dev \
        libttspico-dev

    RUN curl --output libmicrohttpd.tar.gz ftp://ftp.gnu.org/gnu/libmicrohttpd/libmicrohttpd-0.9.63.tar.gz
    RUN tar xzf libmicrohttpd.tar.gz
    RUN cd libmicrohttpd-* && ./configure && make && make install


music:
    FROM +deps
    WORKDIR /code
    COPY Makefile use-pi-def.sh use-pi-lib.sh .
    COPY --dir src .
    RUN make music-server
    SAVE ARTIFACT music-server AS LOCAL music-server
