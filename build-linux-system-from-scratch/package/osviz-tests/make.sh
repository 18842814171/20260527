#!/usr/bin/bash
source "$(dirname "$0")/../common.sh"

PKGNAME=osviz-tests
PKGVERSION=
PKGSOURCE_DIR=
PKGSOURCE=

PKGBUILDNAME=${PKGNAME}
PKGBUILD_DIR=${BUILD_DIR}/${PKGBUILDNAME}

echo "----> Building ${PKGBUILDNAME} ..."

stamp_downloaded
stamp_extracted
stamp_patched
stamp_configured
stamp_built

step_start install-target
mkdir -p "${TARGET_DIR}/root"
/usr/bin/install -D -m 0644 "${PROJECT_DIR}/package/${PKGNAME}/hello.c" "${TARGET_DIR}/root/hello.c"
step_end install-target

stamp_installed

echo "<---- ${PKGBUILDNAME} build complete."
