#!/usr/bin/env python3
from PIL import Image
import numpy as np
import os
import json

def extract_characters(chr_file, output_dir):
    """
    Extract character bitmaps from willy.chr into individual 128x128 PNG files with transparency
    """
    # Character mapping from the original code
    namedpart = {
        "0": "WILLY_RIGHT", 
        "1": "WILLY_LEFT", 
        "2": "PRESENT", 
        "3": "LADDER",
        "4": "TACK", 
        "5": "UPSPRING", 
        "6": "SIDESPRING", 
        "7": "BALL", 
        "8": "BELL"
    }
    # Add pipe parts 51-90
    for i in range(51, 91):
        namedpart[str(i)] = f"PIPE{i-50}"
    namedpart["126"] = "BALLPIT"
    namedpart["127"] = "EMPTY"

    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Read the character data file
    with open(chr_file, 'rb') as f:
        data = bytearray(f.read())

    # Process each character from original 8x8 format
    for i in range(len(data) // 8):
        # Convert 8 bytes into 8x8 bitmap with alpha
        bitmap = np.zeros((8, 8, 4), dtype=np.uint8)
        for y in range(8):
            byte = data[i * 8 + y]
            for x in range(8):
                pixel = (byte >> (7 - x)) & 1
                if pixel:
                    # White pixel with full opacity
                    bitmap[y, x] = [255, 255, 255, 255]
                else:
                    # Black pixel with full transparency
                    bitmap[y, x] = [0, 0, 0, 0]

        # Create PIL image with alpha channel
        img = Image.fromarray(bitmap, 'RGBA')

        # Scale up to 128x128 using nearest neighbor to preserve sharp edges
        img = img.resize((128, 128), Image.NEAREST)

        # Save the image if it has a name
        char_name = namedpart.get(str(i))
        if char_name:
            img.save(os.path.join(output_dir, f"{char_name}.png"), 'PNG')

def create_chr_file(input_dir, output_file):
    """
    Create a new willy.chr file from 128x128 RGBA PNG images.
    New format: JSON header with character mapping + concatenated raw RGBA data
    """
    # Character mapping (reversed)
    namedpart_reverse = {
        "WILLY_RIGHT": 0,
        "WILLY_LEFT": 1,
        "PRESENT": 2,
        "LADDER": 3,
        "TACK": 4,
        "UPSPRING": 5,
        "SIDESPRING": 6,
        "BALL": 7,
        "BELL": 8
    }
    # Add pipe parts
    for i in range(1, 41):
        namedpart_reverse[f"PIPE{i}"] = 50 + i
    namedpart_reverse["BALLPIT"] = 126
    namedpart_reverse["EMPTY"] = 127

    # Format metadata
    metadata = {
        "version": 2,
        "width": 128,
        "height": 128,
        "channels": 4,  # RGBA
        "characters": namedpart_reverse
    }

    # Initialize output data
    header_bytes = json.dumps(metadata).encode('utf-8')
    header_size = len(header_bytes)
    header_size_bytes = header_size.to_bytes(4, byteorder='little')

    # Open output file
    with open(output_file, 'wb') as f:
        # Write header size and header
        f.write(header_size_bytes)
        f.write(header_bytes)

        # Process each character
        max_char_index = max(namedpart_reverse.values()) + 1
        for char_index in range(max_char_index):
            # Find character name if it exists
            char_name = None
            for name, idx in namedpart_reverse.items():
                if idx == char_index:
                    char_name = name
                    break

            if char_name and os.path.exists(os.path.join(input_dir, f"{char_name}.png")):
                # Load image
                img = Image.open(os.path.join(input_dir, f"{char_name}.png"))
                img = img.convert('RGBA')
                if img.size != (128, 128):
                    img = img.resize((128, 128), Image.NEAREST)
                
                # Write raw pixel data
                f.write(img.tobytes())
            else:
                # Write empty character (transparent)
                empty_data = bytes([0] * (128 * 128 * 4))
                f.write(empty_data)

def print_character_info():
    """Print available character names and their indices"""
    namedpart = {
        0: "WILLY_RIGHT", 
        1: "WILLY_LEFT", 
        2: "PRESENT", 
        3: "LADDER",
        4: "TACK", 
        5: "UPSPRING", 
        6: "SIDESPRING", 
        7: "BALL", 
        8: "BELL"
    }
    # Add pipe parts 51-90
    for i in range(51, 91):
        namedpart[i] = f"PIPE{i-50}"
    namedpart[126] = "BALLPIT"
    namedpart[127] = "EMPTY"
    
    print("\nAvailable characters:")
    for idx, name in sorted(namedpart.items()):
        print(f"{idx}: {name}")

def read_chr_file(chr_file):
    """
    Read a chr file and return the metadata and image data.
    Supports both old (8x8) and new (128x128) formats.
    """
    with open(chr_file, 'rb') as f:
        # Try to read as new format first
        try:
            header_size = int.from_bytes(f.read(4), byteorder='little')
            header_data = f.read(header_size)
            metadata = json.loads(header_data.decode('utf-8'))
            
            # It's a new format file
            if metadata["version"] == 2:
                return metadata, f.read()
        except:
            # If that fails, assume it's an old format file
            f.seek(0)
            return None, f.read()

if __name__ == '__main__':
    import sys
    
    if len(sys.argv) < 2 or sys.argv[1] not in ['extract', 'create', 'info']:
        print("Usage:")
        print("  Extract: python willy_chr_tools.py extract <willy.chr> <output_dir>")
        print("  Create:  python willy_chr_tools.py create <input_dir> <output.chr>")
        print("  Info:    python willy_chr_tools.py info")
        sys.exit(1)

    if sys.argv[1] == 'extract':
        if len(sys.argv) != 4:
            print("Extract usage: python willy_chr_tools.py extract <willy.chr> <output_dir>")
            sys.exit(1)
        extract_characters(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == 'create':
        if len(sys.argv) != 4:
            print("Create usage: python willy_chr_tools.py create <input_dir> <output.chr>")
            sys.exit(1)
        create_chr_file(sys.argv[2], sys.argv[3])
    else:
        print_character_info()
