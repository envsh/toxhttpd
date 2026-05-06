#!/bin/bash
set -e

echo "=== Setting up Qt4 build environment ==="

# 检测容器/系统版本
. /etc/os-release
echo "Container: $PRETTY_NAME"

if [[ "$VERSION_ID" == "20.04" ]]; then
    echo "Installing Qt4 from PPA for Ubuntu 20.04..."
    apt-get update
    apt-get install -y software-properties-common
    add-apt-repository ppa:rock-core/qt4 -y
    apt-get update
    apt-get install -y qt4-default libqt4-dev qt4-qmake
elif [[ "$VERSION_ID" == "18.04" ]]; then
    echo "Installing Qt4 from official repos for Ubuntu 18.04..."
    apt-get update
    apt-get install -y qt4-default libqt4-dev qt4-qmake
else
    echo "Unsupported version: $VERSION_ID"
    exit 1
fi

# 验证安装
qmake -v

echo "=== Qt4 environment ready ==="
exit 0
