#!/usr/bin/env bash
set -e
export SRC_DIR=${TOPDIR}
export RES_DIR=${TOPDIR}/results
export ISO_DIR=${RES_DIR}/iso
export PACKAGES_TO_INSTALL="barco-ems"
export SYSTEM_NAME=ems
export SKIP_BUILD_DEPS=TRUE
export LIST_OF_VARS_TO_REPLACE='${BLACKLISTED_PACKAGES},${LOCAL_CD},${CD_VERSION},${PACKAGES_TO_INSTALL},${SYSTEM_NAME},${BASEOS_DISTRO_REPO},${DEBIAN_DISTRO}'
export ISO_CFG=${SYSTEM_NAME,,}-${BASEOS_DISTRO_REPO}
export PACKAGING_SWITCHES="-g -s"
export DO_APT_UPGRADE=
export BARCO_PACKAGE_BUILDER=binary-docker.barco.com/eis/baseos9-package-builder:1.2.2-add-iso-signing-key-67bbc38eaa-dirty
