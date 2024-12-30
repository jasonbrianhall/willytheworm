#!/bin/bash

echo "Very important, use NDK 25 or this will fail"

pip install -r requirements-android.txt 

#p4a clean_all

p4a create --requirements=kivy,sdl2 --bootstrap=sdl2 --arch=x86_64 \
    --package=org.test.willytheworm --name="WillyTheWorm" \
    --version=1.0 --private=$(pwd)/willy.py \
    --sdk-dir=$HOME/Android/Sdk \
    --blacklist-requirements=android
