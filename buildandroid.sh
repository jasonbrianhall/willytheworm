#!/bin/bash

pip install -r requirements-android.txt 
p4a create --requirements=kivy,sdl2 --bootstrap=sdl2 --arch=x86_64 --package=org.willytheworm --name="Willy The Worm" --version=1.0 --private=$(pwd)/willy.py --sdk-dir=$(pwd)/Android/Sdk
