#!/usr/bin/bash
source $(dirname "$0")/../common.sh

PKGNAME=binutils
PKGVERSION=2.43.1
PKGSOURCE_DIR=binutils
PKGSOURCE=binutils-2.43.1.tar.xz
PKGURL=http://mirrors.ustc.edu.cn/gnu/binutils/binutils-2.43.1.tar.xz

PKGBUILDNAME=${PKGNAME}-target
PKGBUILD_DIR=${BUILD_DIR}/${PKGBUILDNAME}-${PKGVERSION}

echo "----> Building ${PKGBUILDNAME} ..."

stamp_downloaded

step_start extract
mkdir -p ${PKGBUILD_DIR}
xzcat ${DL_DIR}/${PKGSOURCE_DIR}/${PKGSOURCE} | tar --strip-components=1 -C ${PKGBUILD_DIR} -xf -
chmod -R +rw ${PKGBUILD_DIR}
step_end extract

step_start patch
TAR="tar" PATH=${HOST_DIR}/bin:$PATH ${PROJECT_DIR}/support/scripts/apply-patches.sh ${PKGBUILD_DIR} ${PROJECT_DIR}/package/${PKGNAME}/${PKGVERSION} \*.patch
step_end patch autotools

step_start configure
(cd ${PKGBUILD_DIR} && rm -rf config.cache; eval "${TARGET_CONFIGURE_OPTS} MAKEINFO=true CONFIG_SITE=/dev/null ./configure --target=${GNU_TARGET_NAME} --host=${GNU_TARGET_NAME} --build=x86_64-pc-linux-gnu --prefix=/usr --sysconfdir=/etc --localstatedir=/var --disable-nls --disable-multilib --disable-werror --disable-sim --disable-gdb --disable-gprofng --without-zstd --enable-poison-system-directories --enable-plugins --enable-lto --with-sysroot=/ --program-prefix=\"\" --disable-shared --enable-static")
step_end configure

step_start build
eval "${TARGET_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} MAKEINFO=true -C ${PKGBUILD_DIR}"
step_end build

step_start install-target
eval "${TARGET_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} MAKEINFO=true DESTDIR=${TARGET_DIR} install -C ${PKGBUILD_DIR}"
step_end install-target

stamp_installed

echo "<---- ${PKGBUILDNAME} build complete."
