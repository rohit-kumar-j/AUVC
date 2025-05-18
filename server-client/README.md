# External Image

Download deps:
```bash
# stb image deps for image compilation
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
wget https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

#Image and video files
wget -O video.mp4 "https://file-examples.com/storage/fe0707c5116828d4b9ad356/2017/04/file_example_MP4_1920_18MG.mp4"
wget -O image.jpg "https://as2.ftcdn.net/v2/jpg/08/63/11/87/1000_F_863118715_1TSqy5McrxtiJf6xY2CkM46pgMEJJ5HP.jpg"
```

NOTE: When you download the following, make sure you change the include(-I) and
the linking(-L) directories in the Makefile when you compile,else just add to
path or source the bashrc or restart the terminal

---

NOTE: Check server ip when compiling

---

# Deps

## MacOS
```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install zeromq glfw ffmpeg
```

## Ubuntu
```bash
# Update package manager
sudo apt update

# Install dependencies
sudo apt install -y libzmq3-dev libglfw3-dev libgl1-mesa-dev ffmpeg libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
```

## Fedora/RHEL
```bash
# Install dependencies
sudo dnf install -y zeromq-devel glfw-devel mesa-libGL-devel ffmpeg-devel
```

## Windows
```bash
# idk... suffer... coz I won't figure it out for you... sorry.
```
---

