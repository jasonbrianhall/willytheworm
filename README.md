# 🐛 Willy the Worm: A Retro Gaming Adventure! 🕹️

![Willy the Worm Banner](images/intro.png)

## 🌟 Dive into the Quirky World of Willy! 🌟

Ever wondered what it's like to be a worm with a passion for adventure? Look no further! **Willy the Worm** is here to squirm into your heart and provide hours of nostalgic fun!

### 🎮 What's the Buzz About?

Willy isn't your average earthworm. He's a:
- 🪜 Ladder-climbing extraordinaire
- 🦘 Spring-bouncing champion
- 🎁 Present-hunting aficionado
- 🔔 Bell-ringing enthusiast

But watch out! Willy's world is full of perilous tacks and mischievous bouncing balls. Can you help him navigate through the challenges?

![Level 1 Screenshot](images/level1.png)

### 🕹️ How to Play

- Use arrow keys ↤ ↥ ↦ ↧ to guide Willy
- Smash that spacebar to make him jump
- Any other key? Willy takes a breather!

### 🚀 Available Versions

Choose your adventure platform:

#### 🐍 Python Version (Original)
Classic cross-platform Python implementation with Qt GUI
```bash
pip install -r requirements.txt
./willy.py
```

#### 🖥️ C++ Linux Version (Native Performance); Windows compile is still experimental (but will compile and run; it's just funky)
High-performance native C++ version with GTK interface
```bash
cd cpp/
make
./build/linux/willy
```

#### 📱 Qt GUI Version
User-friendly GUI for the command-line challenged
```bash
./willy_qt.py
```

### 🚀 Features That'll Make You Go "Wow!"

- 📺 Fullscreen toggle with F11
- 🌈 Dynamic screen color changes (F5, F6, F7)
- 🎹 WASD support for the cool kids
- ⚡ Adjustable game speed
- 🖱️ Mouse support for the pointer-inclined
- 🔊 Sound effects with toggle support
- 🏆 High score tracking (daily and all-time)

### 🛠️ Customization at Your Fingertips

#### Python Version Options:
```
./willy.py 
    -l level           (Start at a specific level)
    -b numberofballs   (Adjust ball count)
    -w                 (Enable WASD controls)
    -f                 (Disable flash effects)
    -F framespersecond (Speed up or slow down)
    -m                 (Enable mouse support)
    -h or --help       (Show all options)
```

#### C++ Linux Version Options:
```
./cpp/build/linux/willy
    -l LEVEL          (Start at specific level)
    -L LEVELFILE      (Use custom levels file)
    -b BALLS          (Set number of balls)
    -w                (Use WASD keyboard controls)
    -f                (Disable death flash effect)
    -F FPS            (Set frames per second)
    -m                (Enable mouse support)
    -s                (Start with sound disabled)
    -S SCALE          (Set scale factor)
    -h, --help        (Show help message)
```

### 🎨 Unleash Your Creativity with the Level Editor!

Think you can create the ultimate Willy challenge? Fire up `edwilly.py` and let your imagination run wild!

Editor Hotkeys:
- F11: Toggle fullscreen
- S: Save your masterpiece
- L: Jump to next level
- Left click: Place objects
- Right click: Remove objects
- Scroll: Cycle through items
- P: Playtest your creation

### 🌟 What Makes a Willy Level Shine?

Every great level needs:
1. Willy (of course!)
2. Two or more ball pits (the last one placed is where the action starts).  Balls not getting stuck.
3. A bell (Willy's ultimate goal!)
4. Your creative touch of obstacles and challenges!
5. Winnability.  Make it winnable.

### 🎵 A Symphony of Nostalgia

While we've updated the audio for modern ears, the spirit of the original game lives on. We've even added an option to disable screen flashes for our photosensitive friends!

### 🎨 Customize Willy's Look with HD Sprites!

Want to give Willy and friends a high-definition makeover? Check out the HD Sprite Tool in `hd/hd.py`! Transform those classic 8x8 sprites into glorious 128x128 HD versions.

Quick Start with HD Sprites:
```bash
# Extract existing sprites
pip install -r hd/requirements.txt # (one time only)
python hd/hd.py extract --input willy.chr --output hd/sprites/

# Customize sprites in your favorite image editor
# Create new HD chr file
python hd/hd.py create --input hd/sprites/ --output willy.chr
```

### 🔧 Build Requirements

#### For Python Version:
- Python 3.6+
- Dependencies listed in `requirements.txt`

#### For C++ Linux Version:
- GCC or Clang compiler
- GTK3 development libraries
- gtkmm-3.0 development libraries
- SDL2 and SDL2_mixer development libraries
- Cairo development libraries
- pkg-config

On Ubuntu/Debian:
```bash
sudo apt-get install build-essential libgtkmm-3.0-dev libsdl2-dev libsdl2-mixer-dev libcairo2-dev pkg-config
```

### 🚀 Ready to Dive In?

1. Clone this repo
2. Choose your preferred version:
   - **Python**: Run `pip install -r requirements.txt`, then `./willy.py`
   - **C++ Linux**: Run `cd cpp && make`, then `./build/linux/willy`
3. Start your wormy adventure!

### 🌐 Spread the Willy Love!

Enjoyed your wormy adventure? Don't keep it to yourself:
- 🌟 Star this repo
- 🍴 Fork it and add your twist
- 📣 Share it with fellow retro gaming enthusiasts

### 👥 Join the Willy Community!

Created something cool? Found a bug? Want to chat about worm physics? Open an issue or submit a pull request. Let's make Willy the talk of the town!

### 📚 Additional Resources

- [Wiki](wiki.md): Detailed game mechanics, characters, items, and level design tips
- [Contribution Guide](contribution.md): Learn how to contribute to Willy the Worm

---

🎉 **Willy the Worm** - Bringing joy to invertebrate enthusiasts since 2023! 🎉

*Original Pascal version by Alan Farmer (1985)*  
*Python reincarnation by Jason Hall (jasonbrianhall@gmail.com)*  
*C++ Linux port by Jason Hall*

This code is FOSS (Free and Open-Source Software). Remix it, share it, love it! 💖

### 📣 Find Us on SourceForge

https://sourceforge.net/projects/willy-the-worm

---

**Keywords**: retro gaming, platformer, Python game, C++ game, Linux game, open-source game, Willy the Worm, level editor, nostalgic games, indie game, 2D game, arcade-style game, GTK, native performance
