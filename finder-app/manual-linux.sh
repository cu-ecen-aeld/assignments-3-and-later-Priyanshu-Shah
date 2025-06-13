#!/bin/bash
if command -v aarch64-none-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE=aarch64-none-linux-gnu-
    echo "Using aarch64-none-linux-gnu-gcc as cross-compiler"
elif command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    CROSS_COMPILE=aarch64-linux-gnu-
    echo "Using aarch64-linux-gnu-gcc as cross-compiler"
fi

# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_35_0
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64

echo "=== DEBUG INFO ==="
echo "PATH is: $PATH"
which ${CROSS_COMPILE}gcc || echo "${CROSS_COMPILE}gcc not found"
echo "PATH is: $PATH"
ls -l $(command -v ${CROSS_COMPILE}gcc || echo "/dev/null")
whoami
id
${CROSS_COMPILE}gcc -v || echo "${CROSS_COMPILE}gcc not working"
echo "==================="

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin usr/lib64
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone https://github.com/mirror/busybox.git busybox
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
    sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config
else
    cd busybox
fi

# TODO: Make and install busybox
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install
ln -sf busybox ${OUTDIR}/rootfs/bin/sh

echo "Library dependencies"
# TODO: Add library dependencies to rootfs
${CROSS_COMPILE}readelf -a busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a busybox | grep "Shared library"

SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -a $SYSROOT/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib64/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib/libc.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib64/libc.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib/libresolv.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib64/libresolv.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib/libm.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true
cp -a $SYSROOT/lib64/libm.so.* ${OUTDIR}/rootfs/lib/ 2>/dev/null || true

# TODO: Make device nodes
cd ${OUTDIR}/rootfs
mkdir -p dev
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
mkdir -p ${OUTDIR}/rootfs/home/conf
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/conf/assignment.txt ${OUTDIR}/rootfs/home/conf/
cp ${FINDER_APP_DIR}/conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/
sed -i 's|\.\./conf/assignment.txt|conf/assignment.txt|g' ${OUTDIR}/rootfs/home/finder-test.sh

chmod +x ${OUTDIR}/rootfs/home/finder.sh
chmod +x ${OUTDIR}/rootfs/home/finder-test.sh
chmod +x ${OUTDIR}/rootfs/home/writer
chmod +x ${OUTDIR}/rootfs/home/autorun-qemu.sh

# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root | gzip > ${OUTDIR}/initramfs.cpio.gz