# 🎮 Willy the Worm HD Sprite Tool

Transform your favorite retro worm into glorious high definition! This tool lets you breathe new life into Willy the Worm by converting the classic 8x8 character sprites into crystal-clear 128x128 HD versions.

## ✨ Features

- **Extract Mode**: Pull sprites from the original `willy.chr` file and save them as individual PNG files
- **Create Mode**: Build a new HD-compatible character file from your customized PNG sprites
- **Perfect Pixel Scaling**: Maintains the crisp, retro aesthetic while bringing Willy into the modern era
- **Full Transparency Support**: Create sprites with clean edges and no background artifacts
- **Character Info**: Quick reference for all available sprite types and their indices

## 🎯 Quick Start

### Extract the original sprites:
```bash
python hd.py extract willy.chr sprites_folder/
```

### Create a new HD character file:
```bash
python hd.py create sprites_folder/ willy_hd.chr
```

### View available character types:
```bash
python hd.py info
```

## 🎨 Customization

Want to give Willy a makeover? Here's what you can customize:

- WILLY_RIGHT/LEFT: Our heroic worm in both directions
- PRESENT: The collectible gifts
- LADDER: Climbing apparatus
- TACK: Those pesky hazards
- UPSPRING/SIDESPRING: The bouncy bits
- BALL: Rolling threats
- BELL: Level completion bells
- PIPE1-40: Various pipe segments
- BALLPIT: Where the balls emerge
- And more!

Each sprite can be edited in your favorite image editor as a 128x128 PNG file with transparency support.

## 🛠️ Technical Details

The tool supports two formats:
- Classic 8x8 bitmap format from the original game
- New HD format with 128x128 RGBA sprites and JSON metadata

The HD format includes:
- Version identifier
- Image dimensions
- RGBA channel support
- Character mapping data
- Raw pixel data for each sprite

## 🌟 Pro Tips

1. Keep your pixel art aligned to an 8x8 grid for that authentic retro feel
2. Use transparency for clean sprite edges
3. Test your sprites in-game to ensure they blend well with the gameplay
4. Back up your original `willy.chr` file before making changes

## 🎮 Compatibility

Your HD sprites will work with the latest version of Willy the Worm while maintaining perfect compatibility with the classic game's mechanics. The game automatically detects and uses the HD sprites when available!

## 🚀 Get Started

1. Clone this repository
2. Extract the original sprites
3. Edit or replace the PNGs in your sprites folder
4. Create a new HD character file
5. Replace the original `willy.chr` with your new version
6. Enjoy Willy in HD glory!

Give Willy the high-definition treatment he deserves! Happy sprite crafting! 🪱✨