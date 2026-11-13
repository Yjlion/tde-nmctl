#!/bin/sh
# Renders the SVG sources into ../../icons (needs rsvg-convert and gzip).
cd "$(dirname "$0")" || exit 1
for f in *.svg; do
	n=${f%.svg}
	gzip -9 -n -c "$f" > ../../icons/hisc-app-$n.svgz
	sizes="16 22 32 48"
	[ "$n" = tde-nmctl ] && sizes="16 22 32 48 64 128"
	for s in $sizes; do
		rsvg-convert -w $s -h $s "$f" -o ../../icons/hi$s-app-$n.png
	done
done
