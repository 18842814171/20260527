#!/usr/bin/bash
source $(dirname "$0")/../common.sh

PKGNAME=mpfr
PKGVERSION=4.1.1
PKGSOURCE_DIR=mpfr
PKGSOURCE=mpfr-4.1.1.tar.xz
PKGURL=http://www.mpfr.org/mpfr-4.1.1/mpfr-4.1.1.tar.xz

PKGBUILDNAME=${PKGNAME}-target
PKGBUILD_DIR=${BUILD_DIR}/${PKGBUILDNAME}-${PKGVERSION}

echo "----> Building ${PKGBUILDNAME} ..."

stamp_downloaded

step_start extract
mkdir -p ${PKGBUILD_DIR}
xzcat ${DL_DIR}/${PKGSOURCE_DIR}/${PKGSOURCE} | tar --strip-components=1 -C ${PKGBUILD_DIR} -xf -
chmod -R +rw ${PKGBUILD_DIR}
step_end extract

stamp_patched autotools

step_start configure
(cd ${PKGBUILD_DIR} && rm -rf config.cache; eval "${TARGET_CONFIGURE_OPTS} CONFIG_SITE=/dev/null ./configure --host=${GNU_TARGET_NAME} --build=x86_64-pc-linux-gnu --prefix=/usr --enable-shared --disable-static --disable-nls --with-gmp=${TARGET_DIR}/usr")
step_end configure

step_start build
eval "${TARGET_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} -C ${PKGBUILD_DIR}"
step_end build

step_start install-target
eval "${TARGET_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} DESTDIR=${TARGET_DIR} install -C ${PKGBUILD_DIR}"
step_end install-target

stamp_installed

echo "<---- ${PKGBUILDNAME} build complete."
