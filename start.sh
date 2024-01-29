rm -rf alkulife
rm -rf doros_alkulify
rm -rf swteat_k
mkdir alkulife
mkdir doros_alkulify
mkdir swteat_k
./fpi-server -d alkulife --http-port 6081 &
./fpi-server -d doros_alkulify --http-port 6083 &
./fpi-server -d swteat_k --http-port 6084
