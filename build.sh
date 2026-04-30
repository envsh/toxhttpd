set -x

source /opt/obggcc4.8/build/autotools/x86_64-unknown-linux-gnu2.17.sh
unset PKG_CONFIG

export CGO_CFLAGS=-I/opt/oldlibc-devsys/include
# static use more -l
export CGO_LDFLAGS="-L/opt/oldlibc-devsys/lib -ltoxav -ltoxencryptsave -lopus -lsodium -lm"

cd go-toxhttpd/
CGO_ENABLED=1 go build -v
