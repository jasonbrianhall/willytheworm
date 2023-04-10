Here are a list of known issues that may or may not get fixed:

1)  Number of balls should be toggable.  Thought about adding as a command line parameter.
2)  Scaler assumes a wide screen resolution.  The original Willy the worm graphics were 8x8 and I use a 4x scaler to make them 32.  Scaler can only be an integer so bigger number will make the screen bigger and a smaller number will make the screen smaller.  It changes the resolution accordingly but hasn't been tested thorougly.  
3)  If you jump on a ladder, you fall until you press a key.  Slightly annoying.
4)  If you jump on a side spring, you get left/right willy animation.  Kind of like the bug honestly.
5)  Movement is based on Frames per second, which is set to 10 by default;  lowering this number makes the game go slower and increasing it make it go faster.  This could be used to implement a "turbo" mode.
6)  Should be a way to use monochrome.  I assume everybody likes color but I'm sure not everybody does.  Easy to implement but I haven't.
7)  Willy jumps higher then the original version but I set it to 3 and didn't like the setting (it's set to 4)
8)  When you jump over balls, your score only increases when you are going up, not down.  Doesn't really count the score correctly but it's only 20 points.
9)  Daily scores are based on when the file got last accessed so if someone wanted to cheat and get yesterdays scores today, they could run the command "touch willy.scr" before they played the game (but who really cares)
10)  I had a double free issue with the sound but that's been fixed (I was loading each sound everytime I used it, now I load them into a dictionary on first access and use locks to keep two threads from loading the same sound)
11)  Level data gets reloaded when the player dies.  Not a big deal but if someone edited the level while they played the game and they died, they could get whole new levels.


