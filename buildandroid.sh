#!/bin/bash

echo "Very important, use NDK 25 or this will fail"

pip install -r requirements-android.txt 
#export ANDROID_NDK_HOME=~/Android/Sdk/ndk/25.1.8937393

#p4a clean_all


cp willy.py main.py -f

p4a apk --requirements=kivy,sdl2,pillow \
    --bootstrap=sdl2 \
    --arch=x86_64 \
    --package=org.willytheworm \
    --name="WillyTheWorm" \
    --version=1.0 \
    --private=$(pwd) \
    --sdk-dir=$HOME/Android/Sdk \
    --dist-name willytheworm \
    --ignore-setup-py

