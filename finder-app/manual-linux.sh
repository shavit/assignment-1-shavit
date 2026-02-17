#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if ! OUTDIR=$(realpath -m "$OUTDIR" 2>/dev/null); then
    echo "Could not validate path ${1}"
    exit 1
fi

if ! mkdir -p "$OUTDIR"; then
    echo "Could not create directory ${OUTDIR}"
    exit 1
fi

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

    make mrproper
    make CROSS_COMPILE="${CROSS_COMPILE}" ARCH="${ARCH}" defconfig
    make -j "$(nproc)" CROSS_COMPILE="${CROSS_COMPILE}" ARCH="${ARCH}" all
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "${OUTDIR}"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi


mkdir "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr usr/bin usr/lib usr/sbin var var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    
    make CROSS_COMPILE="${CROSS_COMPILE}" ARCH="${ARCH}" defconfig
else
    cd busybox
fi

make -j "$(nproc)" CROSS_COMPILE="${CROSS_COMPILE}" ARCH="${ARCH}"
make CONFIG_PREFIX="${OUTDIR}/rootfs" CROSS_COMPILE="${CROSS_COMPILE}" ARCH="${ARCH}" install
cp busybox "${OUTDIR}/rootfs/bin"
# ln -sf "${OUTDIR}/rootfs/bin/busybox" "${OUTDIR}/rootfs/init"
chmod 4755 "${OUTDIR}/rootfs/bin/busybox"


echo "Library dependencies"
${CROSS_COMPILE}readelf -a busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a busybox | grep "Shared library"


cp "$(aarch64-none-linux-gnu-gcc -print-file-name=ld-linux-aarch64.so.1)" "${OUTDIR}/rootfs/lib"
rlibs=("libm.so.6" "libresolv.so.2" "libc.so.6")
for x in "${rlibs[@]}"; do
    libpath=$(aarch64-none-linux-gnu-gcc -print-file-name="${x}")
    cp "${libpath}" "${OUTDIR}/rootfs/lib64"
done


cd "${OUTDIR}/rootfs/dev"
sudo mknod -m 666 null c 1 3
sudo mknod -m 600 console c 5 1
# sudo mknod -m 666 tty c 5 0


cd "${FINDER_APP_DIR}"
make CROSS_COMPILE="${CROSS_COMPILE}" clean
make CROSS_COMPILE="${CROSS_COMPILE}" build


cd "${OUTDIR}/rootfs"
find "${FINDER_APP_DIR}" -maxdepth 1 -type f -exec cp {} "${OUTDIR}/rootfs/home/"  \;
mkdir "${OUTDIR}/rootfs/home/conf"
find "${FINDER_APP_DIR}/conf/" -maxdepth 1 -type f -exec cp {} "${OUTDIR}/rootfs/home/conf/"  \;


sudo chown -R 0:0 "${OUTDIR}/rootfs/"


cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner 0:0 > "${OUTDIR}/initramfs.cpio"
cd "${OUTDIR}"
gzip -f initramfs.cpio
