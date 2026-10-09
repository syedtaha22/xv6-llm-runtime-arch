#!/usr/bin/env bash
# Builds the Linux baseline.
#
# Output in ./out: Image (riscv64 Linux kernel), run (llama2.c), init (benchmark runner),
# bench.txt (list of runs) and initramfs.cpio.gz (init, run, shared libraries, model, tokenizer
# and the list of runs).
#
# Requirements: riscv64-linux-gnu-gcc with the riscv64 libgomp and libc shared libraries,
# curl, cpio, gzip and python3.

set -euo pipefail
cd "$(dirname "$0")"

MODELS=../../server/models
KERNEL_DEB=linux-image-6.8.0-60-generic_6.8.0-60.63.1_riscv64.deb
KERNEL_URL=http://ports.ubuntu.com/ubuntu-ports/pool/main/l/linux-riscv/$KERNEL_DEB

# Fetch a file unless it exists. The download gets a temporary name so that an interrupted
# download leaves nothing behind.
download() {
  local url=$1
  local path=$2

  [ -f "$path" ] && return

  curl -sfL "$url" -o "$path.part"
  mv "$path.part" "$path"
}

# Build the benchmark program with the same -O flag as the xv6 build, and the init program.
build_programs() {
  riscv64-linux-gnu-gcc -O -fopenmp -o out/run run.c -lm
  riscv64-linux-gnu-gcc -O -static -o out/init init.c
}

build_kernel() {
  [ -f out/Image ] && return

  download "$KERNEL_URL" "cache/$KERNEL_DEB"

  rm -rf cache/kernel
  mkdir cache/kernel
  dpkg -x "cache/$KERNEL_DEB" cache/kernel

  python3 extract_kernel.py cache/kernel/boot/vmlinuz-* out/Image
}

# Print the names of the shared libraries that an ELF file needs.
needed_libraries() {
  riscv64-linux-gnu-readelf -d "$1" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'
}

# Copy the shared libraries that run needs, and the libraries those need, into the root file
# system.
copy_shared_libraries() {
  local libdir
  local queue=(out/run)
  local seen=" "
  local file
  local lib

  libdir=$(dirname "$(riscv64-linux-gnu-gcc -print-file-name=libc.so.6)")
  mkdir -p cache/rootfs/lib

  while [ ${#queue[@]} -gt 0 ]; do
    file=${queue[0]}
    queue=("${queue[@]:1}")

    for lib in $(needed_libraries "$file"); do
      case "$seen" in *" $lib "*) continue ;; esac

      seen="$seen$lib "
      cp -L "$libdir/$lib" "cache/rootfs/lib/$lib"
      queue+=("$libdir/$lib")
    done
  done
}

build_initramfs() {
  rm -rf cache/rootfs
  mkdir -p cache/rootfs/proc cache/rootfs/dev

  cp out/init cache/rootfs/init
  cp out/run cache/rootfs/run
  copy_shared_libraries

  cp out/bench.txt cache/rootfs/bench.txt
  cp "$MODELS/stories15M.bin" cache/rootfs/stories15M.bin
  cp "$MODELS/tokenizer.bin" cache/rootfs/tokenizer.bin

  (cd cache/rootfs && find . | cpio -o -H newc --quiet | gzip -1 >../../out/initramfs.cpio.gz)
}

mkdir -p out cache

build_programs
build_kernel
python3 write_run_list.py ../tests.json out/bench.txt
build_initramfs

ls -la out
