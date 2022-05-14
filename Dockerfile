# Build Stage
FROM --platform=linux/amd64 ubuntu:20.04 as builder

## Install build dependencies.
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y autotools-dev make automake clang libsnappy-dev snapd libsnappy1v5

## Add source code to the build stage.
ADD . /snzip
WORKDIR /snzip

## TODO: ADD YOUR BUILD INSTRUCTIONS HERE.
RUN ./autogen.sh && ./configure --disable-dependency-tracking && make

# Package Stage
FROM --platform=linux/amd64 ubuntu:20.04

## TODO: Change <Path in Builder Stage>
COPY --from=builder /snzip/snzip /

