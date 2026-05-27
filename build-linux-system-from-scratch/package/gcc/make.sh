#!/usr/bin/bash
source $(dirname "$0")/../common.sh

PKGNAME=gcc
PKGVERSION=14.3.0
PKGSOURCE_DIR=gcc
PKGSOURCE=gcc-14.3.0.tar.xz
PKGURL=http://mirrors.ustc.edu.cn/gnu/gcc/gcc-14.3.0/gcc-14.3.0.tar.xz

PKGBUILDNAME=${PKGNAME}-target
PKGBUILD_DIR=${BUILD_DIR}/${PKGBUILDNAME}-${PKGVERSION}

# Real cross compiler (not toolchain-wrapper: wrapper reports COLLECT_GCC=/usr/bin/gcc
# and gcc configure then wrongly picks /usr/bin/gcc for host tools).
GCC_HOST_CC="${HOST_DIR}/bin/${GNU_TARGET_NAME}-gcc.br_real"
GCC_HOST_CXX="${HOST_DIR}/bin/${GNU_TARGET_NAME}-g++.br_real"
if [ ! -x "${GCC_HOST_CC}" ]; then
	echo "ERROR: ${GCC_HOST_CC} not found (rebuild host toolchain first)" 1>&2
	exit 1
fi

echo "----> Building ${PKGBUILDNAME} ..."

stamp_downloaded

step_start extract
rm -rf ${PKGBUILD_DIR}
mkdir -p ${PKGBUILD_DIR}
xzcat ${DL_DIR}/${PKGSOURCE_DIR}/${PKGSOURCE} | tar --strip-components=1 -C ${PKGBUILD_DIR} --exclude='libjava/*' --exclude='libgo/*' -xf -
chmod -R +rw ${PKGBUILD_DIR}
step_end extract

step_start patch
TAR="tar" PATH=${HOST_DIR}/bin:$PATH ${PROJECT_DIR}/support/scripts/apply-patches.sh ${PKGBUILD_DIR} ${PROJECT_DIR}/package/${PKGNAME}/14.3.0 \*.patch || exit 1
step_end patch autotools

if [ ! -d "${TARGET_DIR}/usr/include" ]; then
	echo ">>> Restoring ${TARGET_DIR}/usr/include from staging for gcc"
	rsync -a "${STAGING_DIR}/usr/include/" "${TARGET_DIR}/usr/include/"
fi
mkdir -p "${TARGET_DIR}/usr/include"
for h in gmp.h mpfr.h mpc.h; do
	if [ ! -f "${TARGET_DIR}/usr/include/${h}" ] && [ -f "${HOST_DIR}/include/${h}" ]; then
		cp -a "${HOST_DIR}/include/${h}" "${TARGET_DIR}/usr/include/"
	fi
done

step_start configure
mkdir -p ${PKGBUILD_DIR}/build
ln -sf ../configure ${PKGBUILD_DIR}/build/configure
# Canadian cross: build=x86, host=riscv, target=riscv.
# CC must be the *real* riscv cross compiler (.br_real), not toolchain-wrapper.
# Keep PATH minimal so /opt/riscv or other toolchains do not confuse configure.
(cd ${PKGBUILD_DIR}/build && rm -rf config.cache && \
	PATH="${HOST_DIR}/bin:/usr/bin:/bin" \
	CC="${GCC_HOST_CC}" \
	CXX="${GCC_HOST_CXX:-/usr/bin/g++}" \
	MAKEINFO=missing \
	CC_FOR_BUILD=/usr/bin/gcc \
	CXX_FOR_BUILD=/usr/bin/g++ \
	CFLAGS_FOR_BUILD="-O2 -I${HOST_DIR}/include" \
	CXXFLAGS_FOR_BUILD="-O2 -I${HOST_DIR}/include" \
	LDFLAGS_FOR_BUILD="-L${HOST_DIR}/lib" \
	CPPFLAGS="-I${TARGET_DIR}/usr/include" \
	LDFLAGS="-L${TARGET_DIR}/usr/lib" \
	CFLAGS_FOR_TARGET="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -g0" \
	CXXFLAGS_FOR_TARGET="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -g0" \
	LDFLAGS_FOR_TARGET=" " \
	AR_FOR_TARGET=${CROSS_COMPILE}gcc-ar \
	NM_FOR_TARGET=${CROSS_COMPILE}gcc-nm \
	RANLIB_FOR_TARGET=${CROSS_COMPILE}gcc-ranlib \
	./configure \
	--prefix=/usr \
	--build=x86_64-pc-linux-gnu \
	--host=${GNU_TARGET_NAME} \
	--target=${GNU_TARGET_NAME} \
	--with-sysroot=${TARGET_DIR} \
	--with-build-time-tools=${HOST_DIR}/${GNU_TARGET_NAME}/bin \
	--with-gnu-as \
	--with-gnu-ld \
	--with-gmp=${TARGET_DIR}/usr \
	--with-mpfr=${TARGET_DIR}/usr \
	--with-mpc=${TARGET_DIR}/usr \
	--enable-languages=c \
	--disable-multilib \
	--disable-libsanitizer \
	--disable-libvtv \
	--disable-libmpx \
	--disable-libssp \
	--disable-libquadmath \
	--disable-libquadmath-support \
	--disable-decimal-float \
	--disable-libgomp \
	--without-isl \
	--without-cloog \
	--without-zstd \
	--with-arch=rv64imafd_zicsr_zifencei \
	--with-abi=lp64d \
	--enable-shared \
	--enable-threads \
	--enable-tls \
	--enable-__cxa_atexit \
	--disable-bootstrap) || exit 1
# Verify configure picked the cross compiler, not /usr/bin/gcc.
if grep -q "^CC='/usr/bin/gcc'" ${PKGBUILD_DIR}/build/Makefile 2>/dev/null; then
	echo "ERROR: gcc configure still set CC=/usr/bin/gcc (check PATH and ${GCC_HOST_CC})" 1>&2
	exit 1
fi
step_end configure

step_start build
eval "PATH=\"${HOST_DIR}/bin:/usr/bin:/bin\" /usr/bin/make -j${MAXNUM_CPUS} \
	CC=\"${GCC_HOST_CC}\" \
	CXX=\"${GCC_HOST_CXX:-/usr/bin/g++}\" \
	gcc_cv_prog_makeinfo_modern=no gcc_cv_libc_provides_ssp=yes \
	-C ${PKGBUILD_DIR}/build" || exit 1
step_end build

step_start install-target
eval "PATH=\"${HOST_DIR}/bin:/usr/bin:/bin\" /usr/bin/make -j${MAXNUM_CPUS} \
	CC=\"${GCC_HOST_CC}\" \
	CXX=\"${GCC_HOST_CXX:-/usr/bin/g++}\" \
	install-gcc install-target-libgcc DESTDIR=${TARGET_DIR} \
	-C ${PKGBUILD_DIR}/build" || exit 1
rm -f ${TARGET_DIR}/usr/bin/gcov ${TARGET_DIR}/usr/bin/gcov-tool ${TARGET_DIR}/usr/bin/gcov-dump 2>/dev/null || true
if [ -x "${TARGET_DIR}/usr/bin/gcc" ]; then
	file "${TARGET_DIR}/usr/bin/gcc" | grep -q 'RISC-V' || {
		echo "ERROR: ${TARGET_DIR}/usr/bin/gcc is not RISC-V:" 1>&2
		file "${TARGET_DIR}/usr/bin/gcc" 1>&2
		exit 1
	}
	ln -sf gcc ${TARGET_DIR}/usr/bin/cc
else
	echo "ERROR: ${TARGET_DIR}/usr/bin/gcc not found after install" 1>&2
	exit 1
fi
step_end install-target

stamp_installed

echo "<---- ${PKGBUILDNAME} build complete."
