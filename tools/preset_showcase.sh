#!/bin/sh
# Mette in fila gli export dei preset (stessa frase di "Riky's Theme" con ognuno) in un solo MP3 da ascoltare.
# uso: tools/preset_showcase.sh cartella-con-i-wav uscita.mp3
set -e
dir="$1"; out="$2"
list=$(mktemp)
sil=$(mktemp --suffix=.wav)
ffmpeg -v error -y -f lavfi -i anullsrc=r=44100:cl=stereo -t 1.2 -c:a pcm_s24le "$sil"
for f in "$dir"/*.wav; do
  printf "file '%s'\nfile '%s'\n" "$(realpath "$f")" "$sil" >> "$list"
done
# stesso guadagno per tutti (i livelli relativi restano): il picco più alto a -1 dBFS
max=$(ffmpeg -hide_banner -f concat -safe 0 -i "$list" -af volumedetect -f null - 2>&1 | sed -n 's/.*max_volume: \(-*[0-9.]*\) dB/\1/p')
gain=$(awk "BEGIN { print -1 - ($max) }")
echo "guadagno comune: $gain dB"
ffmpeg -v error -y -f concat -safe 0 -i "$list" -af "volume=${gain}dB" -c:a libmp3lame -b:a 320k -metadata title="Riky Piano Man - preset Character Pianos" -metadata artist="Riky Piano Man" "$out"
rm -f "$list" "$sil"
ffprobe -v error -show_entries format=duration -of default=nw=1 "$out"
