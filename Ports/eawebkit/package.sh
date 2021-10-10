#!/usr/bin/env -S bash ../.port_include.sh
port=eawebkit
version=16.4.1.0.2
useconfigure=true
#depends=("libiconv" "libtiff" "xz" "bzip")
commit=a9d899bf68c06e45226141a3ccc6db39f53eec54
workdir="${port}-${commit}"
files="https://github.com/mundak/eawebkit/archive/${commit}.tar.gz ${commit}.tar.gz 0cf3bb214f371e0dd109bbef68aea36e8173bb3f1a2dca53b226f809b03bd8bb"
auth_type="sha256"
#installopts=("INSTALL_TOP=${SERENITY_INSTALL_ROOT}/usr/local")
configopts=("-DCMAKE_TOOLCHAIN_FILE=${SERENITY_BUILD_DIR}/CMakeToolchain.txt -G Ninja")

configure() {
    run mkdir -p build
    run sh -c "cd build && cmake ${configopts[@]} .."
}

build() {
    run sh -c "cd build && ninja"
}
