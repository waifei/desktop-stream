A small program that streams screenshots to web browsers through WebSocket.

Currently, the images are streamed directly without any streaming compression implemented.

Depends on ImageMagick, Boost, OpenSSL, and the linked repositories.

```sh
git clone --recursive https://github.com/eidheim/desktop-stream
cd desktop-stream
cmake .
# You might want to change the const variable values before compilation (see main.cpp)
make
./desktop-stream
```
