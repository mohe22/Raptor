#!/bin/bash
set -e

OS=""
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
fi

echo "Detected OS: $OS"

is_installed_apt()    { dpkg -s "$1" &>/dev/null 2>&1; }
is_installed_pacman() { pacman -Qi "$1" &>/dev/null 2>&1; }
is_installed_dnf()    { rpm -q "$1" &>/dev/null 2>&1; }

install_apt() {
    if is_installed_apt "$1"; then
        echo "[skip] $1 already installed"
    else
        echo "[install] $1"
        sudo apt install -y "$1"
    fi
}

install_pacman() {
    if is_installed_pacman "$1"; then
        echo "[skip] $1 already installed"
    else
        echo "[install] $1"
        sudo pacman -S --noconfirm "$1"
    fi
}

install_dnf() {
    if is_installed_dnf "$1"; then
        echo "[skip] $1 already installed"
    else
        echo "[install] $1"
        sudo dnf install -y "$1"
    fi
}

install_drogon_from_source() {
    if [ -f /usr/local/lib/libdrogon.so ] || [ -f /usr/local/lib64/libdrogon.so ]; then
        echo "[skip] drogon already installed from source"
        return
    fi

    echo "[install] drogon from source (no db backends)"
    local tmp=$(mktemp -d)
    git clone --depth=1 https://github.com/drogonframework/drogon "$tmp/drogon"
    cd "$tmp/drogon"
    git submodule update --init

    mkdir build && cd build
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_POSTGRESQL=OFF \
        -DBUILD_MYSQL=OFF \
        -DBUILD_SQLITE=OFF \
        -DBUILD_REDIS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    cd /
    rm -rf "$tmp"
    echo "[done] drogon installed from source"
}

remove_system_drogon_apt() {
    if is_installed_apt libdrogon-dev; then
        echo "[remove] system drogon (has db backends)"
        sudo apt remove -y libdrogon-dev
    fi
}

remove_system_drogon_pacman() {
    if is_installed_pacman drogon; then
        echo "[remove] system drogon (has db backends)"
        sudo pacman -R --noconfirm drogon
    fi
}

case "$OS" in
    ubuntu|debian|linuxmint|pop)
        sudo apt update
        install_apt libsqlite3-dev
        install_apt git
        install_apt cmake
        install_apt build-essential
        remove_system_drogon_apt
        install_drogon_from_source
        ;;
    arch|manjaro|endeavouros)
        install_pacman sqlite
        install_pacman git
        install_pacman cmake
        install_pacman base-devel
        remove_system_drogon_pacman
        install_drogon_from_source
        ;;
    fedora)
        install_dnf sqlite-devel
        install_dnf git
        install_dnf cmake
        install_dnf gcc-c++
        install_drogon_from_source
        ;;
    centos|rhel|rocky|almalinux)
        install_dnf sqlite-devel
        install_dnf git
        install_dnf cmake
        install_dnf gcc-c++
        install_drogon_from_source
        ;;
    opensuse*|sles)
        sudo zypper install -y sqlite3-devel git cmake gcc-c++
        install_drogon_from_source
        ;;
    *)
        echo "[error] Unknown OS: $OS"
        echo "Install manually:"
        echo "  sqlite3: libsqlite3-dev / sqlite-devel"
        echo "  drogon:  https://github.com/drogonframework/drogon"
        exit 1
        ;;
esac

echo ""
echo "All dependencies installed!"
echo "Now run:"
echo "  cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug"
echo "  cmake --build build/debug -j\$(nproc)"
