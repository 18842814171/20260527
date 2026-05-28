#!/usr/bin/bash
# Incrementally add C++ to the host cross toolchain (needed for Canadian-cross target gcc).
source $(dirname "$0")/../common.sh

PKGNAME=gcc
PKGVERSION=14.3.0
PKGBUILDNAME=host-${PKGNAME}-final
PKGBUILD_DIR=${BUILD_DIR}/${PKGBUILDNAME}-${PKGVERSION}
GXX_REAL="${HOST_DIR}/bin/${GNU_TARGET_NAME}-g++.br_real"

if [ -x "${GXX_REAL}" ]; then
	echo "----> ${GNU_TARGET_NAME}-g++ already present, skipping host g++ build"
	exit 0
fi

if [ ! -d "${PKGBUILD_DIR}/build" ]; then
	echo "ERROR: ${PKGBUILD_DIR}/build missing (run: make toolchain)" 1>&2
	exit 1
fi

echo "----> Building host cross g++ (${GNU_TARGET_NAME}-g++) ..."

step_start configure
(cd ${PKGBUILD_DIR}/build && rm -rf config.cache && eval "${HOST_CONFIGURE_OPTS} CFLAGS=\"-O2 -I${HOST_DIR}/include\" LDFLAGS=\"-L${HOST_DIR}/lib -Wl,-rpath,${HOST_DIR}/lib\" MAKEINFO=missing CFLAGS_FOR_TARGET=\"-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64  -O2 -g0 -D_FORTIFY_SOURCE=1 \" CXXFLAGS_FOR_TARGET=\"-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64  -O2 -g0 -D_FORTIFY_SOURCE=1 \" LDFLAGS_FOR_TARGET=\" \" AR_FOR_TARGET=gcc-ar NM_FOR_TARGET=gcc-nm RANLIB_FOR_TARGET=gcc-ranlib ./configure --prefix=\"${HOST_DIR}\" --sysconfdir=\"${HOST_DIR}/etc\" --enable-static --target=${GNU_TARGET_NAME} --with-sysroot=${STAGING_DIR} --enable-__cxa_atexit --with-gnu-ld --disable-libssp --disable-multilib --disable-decimal-float --enable-plugins --enable-lto --with-gmp=${HOST_DIR} --with-mpc=${HOST_DIR} --with-mpfr=${HOST_DIR} --without-zstd --disable-libquadmath --disable-libquadmath-support --enable-tls --enable-threads --without-isl --without-cloog --with-arch=\"rv64imafd_zicsr_zifencei\" --with-abi=\"lp64d\" --enable-languages=c,c++ --with-build-time-tools=${HOST_DIR}/${GNU_TARGET_NAME}/bin --enable-shared --disable-libgomp") || exit 1
step_end configure

step_start build
eval "${HOST_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} gcc_cv_prog_makeinfo_modern=no gcc_cv_libc_provides_ssp=yes -C ${PKGBUILD_DIR}/build"
step_end build

step_start install-host
eval "${HOST_MAKE_ENV} /usr/bin/make -j${MAXNUM_CPUS} install-gcc install-target-libstdc++-v3 -C ${PKGBUILD_DIR}/build"
if [ ! -e ${HOST_DIR}/bin/toolchain-wrapper ]; then
	/usr/bin/gcc -O2 -I${HOST_DIR}/include -DBR_CROSS_PATH_SUFFIX='".br_real"' -DBR_SYSROOT="\"${STAGING_SUBDIR}\"" -DBR_ADDITIONAL_CFLAGS='"-fstack-protector-strong",' -DBR2_PIC_PIE -DBR2_RELRO_FULL -s -Wl,--hash-style=both ${PROJECT_DIR}/package/${PKGNAME}/toolchain-wrapper.c -o ${HOST_DIR}/bin/toolchain-wrapper
fi
cd ${HOST_DIR}/bin
for i in ${GNU_TARGET_NAME}-g++ ${GNU_TARGET_NAME}-g++-*; do
	[ -e "$i" ] || continue
	case "$i" in
	*.br_real) continue ;;
	esac
	rm -f "$i.br_real"
	mv "$i" "$i.br_real"
	ln -sf toolchain-wrapper "$i"
	ln -sf toolchain-wrapper "${ARCH}-linux${i##${GNU_TARGET_NAME}}"
	ln -snf "$i.br_real" "${ARCH}-linux${i##${GNU_TARGET_NAME}}.br_real"
done
step_end install-host

if [ ! -x "${GXX_REAL}" ]; then
	echo "ERROR: ${GXX_REAL} not found after host g++ build" 1>&2
	exit 1
fi

echo "<---- host cross g++ build complete."
