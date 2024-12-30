#!/bin/bash

help() {
   echo "Usage: $0 [OPTION]"
   echo "Options:"
   echo "  -h,  --help        Show help"
   echo "  -ca, --cleanarm    Clean arm64 build"
   echo "  -cx, --cleanx86    Clean x86_64 build" 
   echo "  -a,  --arm         Build arm64 APK"
   echo "  -x,  --x86         Build x86_64 APK"
}

check_ndk() {
   if [ -z "$ANDROID_NDK_HOME" ]; then
       echo "ANDROID_NDK_HOME not set"
       exit 1
   fi
   
   NDK_VERSION=$(basename "$ANDROID_NDK_HOME" | cut -d'.' -f1)
   if [ "$NDK_VERSION" != "25" ]; then
       echo "NDK version 25 required, found version $NDK_VERSION"
       exit 1
   fi
}

clean() {
   local arch=$1
   p4a clean_all
   rm -rf build-$arch
}

build() {
   local arch=$1
   local build_dir="build-$arch"
   local apk_name="WillyTheWorm-$arch.apk"

   check_ndk
   cp willy.py main.py -f
   p4a apk --requirements=kivy,sdl2,pillow \
       --bootstrap=sdl2 \
       --arch=$arch \
       --package=org.test.willytheworm \
       --name="Willy the Worm" \
       --version=1.0 \
       --private=$(pwd) \
       --sdk-dir=$HOME/Android/Sdk \
       --dist-name willytheworm-$arch \
       --storage-dir=$(pwd)/$build_dir \
       --ignore-setup-py \
       --copy-to ./$apk_name
}

case "$1" in
   "-h"|"--help") help;;
   "-ca"|"--cleanarm") clean "arm64";;
   "-cx"|"--cleanx86") clean "x86_64";;
   "-a"|"--arm") build "arm64-v8a";;
   "-x"|"--x86") build "x86_64";;
   *) help;;
esac
