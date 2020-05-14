set -x
rm -rf *.hex
docker exec -it espruino bash -c "cd /out/Espruino && BOARD=NRF52832DK RELEASE=1 make" 
nrfjprog --program *.hex --sectorerase -r -c 50000
npx espruino prog.js -w