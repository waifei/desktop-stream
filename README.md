A small program that streams screenshots to web browsers through WebSocket.

Currently, the images are streamed directly without any streaming compression implemented.
Screenshots are additionally created and resized using `sh` instead of a library (mainly because of
missing ports of the screen grab part of frequently used libraries (for instance Gtk+, ImageMagick) to OS X). 

Depends on ImageMagick, Boost, OpenSSL, and the linked repositories.

```sh
git clone --recursive https://github.com/eidheim/desktop-stream
cd desktop-stream
cmake .
# You might want to change the const variable values before compilation (see main.cpp)
make
./desktop-stream
```

After running desktop-stream, open [http://localhost:8080](http://localhost:8080).
