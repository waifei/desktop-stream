A small program that streams screenshots of the screen to web browsers through WebSocket.

Currently dependent on ImageMagick, Boost, OpenSSL, and the linked repositories.

```sh
git clone --recursive https://github.com/eidheim/desktop-stream
cd desktop-stream
cmake .
# You might want to change the const variable values before compilation (see main.cpp)
make
./desktop-stream
```
