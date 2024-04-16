#!/bin/bash

# run from edge-tools/autoadd
mkdir -p pkgroot/home/edge/
tar -xzf dist.tar.gz -C pkgroot/home/edge/
mv pkgroot/home/edge/dist pkgroot/home/edge/agent
cd pkgroot/home/edge/agent
mv shared/mtconnect/* ./
rm -rf docker
rm -rf shared
rm -rf simulator
rm -rf demo

ver=$(cat pkgroot/DEBIAN/control | grep Version | cut -d ':' -f 2 | tr -d ' ')
debname="mtconnect-agent-${ver}.deb"
dpkg-deb --build pkgroot ${debname}
rm -rf pkgroot/home
