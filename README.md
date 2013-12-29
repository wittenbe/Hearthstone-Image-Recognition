Hearthstone-Image-Recognition
=============================

A twitch.tv bot to extract card/class data for automated scoring and other shenanigans

So what does this do?
---------------------

The bot aims to support a Hearthstone streamer in various ways by applying several image recognition algorithms.  
There are three main features:  
1. An IRC bot to take commands from the twitch chat and return appropriate responses
2. Automatic decklist creation from arena by observing the picks
3. Automatic game state detection to inform another bot (Valuebot, Hidbot, ..) when a game is over and what the outcome was by sending "!score" commands

How does it do it?
------------------

The IRC bot is based on [this](https://github.com/muriloadriano/cleverbot "") implementation, although it has been heavily modified since to adhere to the twitch environment.

Important for both recognition tasks are perceptual hashes. If you're unsure what this is, click [here](http://www.hackerfactor.com/blog/?/archives/432-Looks-Like-It.html "") for a great writeup, 
although the actual implementation is very close to that of the [pHash library](http://phash.org/ "").  
By precomputing the pHash of every card and hero image and then computing the pHash of selected regions of interest of the stream image, 
the recognition task is reduced to simply calculating the hamming distance of both pHashes and seeing if it's below a predetermined threshold.  
This alone is enough for the card and hero detection, i.e. automated deck building and detection of a game's start.

What's missing is the detection of who's first and second as well as the game's end. This is done by looking at the coin at the start of the game and at the "Defeat!" or "Victory!" text at the end of it.
Unfortunately, the pHashes for both coins and for both Victory/Defeat messages are very similar. In case of a "recognition hit", the image region of interest is further analysed by performing the more expensive
SIFT feature detection, which can easily differentiate between both coins and both texts.

Compiling the bot
=================

First of all, you need [Boost](http://www.boost.org/) 1.54 or later, [OpenCV](http://opencv.org/) 2.4.6 or later, [cmake](http://www.cmake.org/) and a compiler supporting C++11 (i.e. g++-4.7 or later for Linux and the latest [MinGW](http://www.mingw.org/) compiler for Windows).
I haven't tried Microsoft's compiler.

With that alone, you should be able to compile the source (see below). To actually run it, you need to download/install [curl](http://curl.haxx.se/) and [livestreamer](http://livestreamer.tanuki.se/en/latest/)

Compiling on Windows
--------------------

I assume you've compiled boost already.
I strongly recommend compiling OpenCV yourself; I've had nothing but problems with the precompiled binaries. I also recommend getting cmake-gui.
Create a `build` directory in the project's root. Start up cmake-gui and set the source field to the `src` folder and the build field to the `build` folder.  
Afterwards, hit Configure and Generate.

Compiling on Linux
------------------

Compile boost (at least system, regex, chrono and thread) and opencv first. 

From the project's root:  
```
mkdir build
cd build
cmake ../src
make
```
	
Use `ccmake ../src` or cmake-gui to specify any paths cmake didn't find itself.

Configuring the bot
===================

Unfortunately, this is currently fairly difficult. The easy part is in the config.xml (most is self-explanatory), use [this](http://twitchapps.com/tmi/) if you need an oauth token for your bot account.  
As I mentioned before, all calculations are performed on specific regions of interest (ROIs), which need to be fairly precise. You'll see the ROIs for Trump's stream in Recognizer.cpp as constants.
If you want to run the bot anywhere else, you'll very like have to change the values.  
Eventually, I plan on providing a calibration tool and allow the system to use different calibration files, but this isn't ready yet.



