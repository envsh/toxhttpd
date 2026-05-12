#!/bin/bash

set -e fail
set -x

# git clone fork

pwd
PROJDIR=$PWD

git clone -b group-ngc https://github.com/envsh/go-toxcore-c.git go-toxhttpd/go-toxcore-c

wget https://github.com/TokTok/c-toxcore/releases/download/v0.2.22/toxcore-v0.2.22-linux-x86_64.tar.gz

tar xvf toxcore-v0.2.22-linux-x86_64.tar.gz
mv toxcore-linux-x86_64 toxlibs

export CGO_ENABLED=1
export CGO_CFLAGS="-I$PROJDIR/toxlibs/include"
export CGO_LDFLAGS="-L$PROJDIR/toxlibs/lib"
export PKG_CONFIG_PATH=$PROJDIR/toxlibs/lib/pkgconfig

cd go-toxhttpd && ln -sv go.work.impl go.work
# cd go-toxhttpd && go build -v ./...
go build -v

pwd && ls -lh && ldd toxhttpd

# it should be in $PROJDIR/go-toxhttpd/
exit 0
