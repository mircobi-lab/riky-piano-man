#!/bin/sh
b=$(basename "$1" .mid)
/home/claude/ext/sfizz/build/library/bin/sfizz_render --sfz "/home/claude/ext/maestro/Maestro Concert Grand.sfz" --midi "$1" --wav "$b.raw.wav" -s 44100 -q 2 --use-eot >/dev/null 2>&1
ffmpeg -v error -y -i "$b.raw.wav" -c:a aac -b:a 128k "$b.m4a" && ffmpeg -v error -y -i "$b.m4a" -ar 44100 "$b.wav"
