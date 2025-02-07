#!/usr/bin/env python3
from PIL import Image
import numpy as np
import os
import json
import argparse

def extract_characters(chr_file, output_dir, size=128):
    """
    Extract character bitmaps from willy.chr into individual PNG files
    Supports both old 8x8 and new 128x128 formats
    
    Args:
        chr_file: Path to the chr file
        output_dir: Directory to save extracted PNGs
        size: Output size (8 or 128)
    """
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
    for i in range(51, 91):
        namedpart[str(i)] = f"PIPE{i-50}"
    namedpart["126"] = "BALLPIT"
    namedpart["127"] = "EMPTY"

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    metadata, data = read_chr_file(chr_file)
    
    if metadata:  # New format (128x128)
        char_size = metadata["width"] * metadata["height"] * metadata["channels"]
        for i in range(len(data) // char_size):
            char_name = namedpart.get(str(i))
            if char_name:
                start = i * char_size
                end = start + char_size
                pixel_data = data[start:end]
                img = Image.frombytes('RGBA', (128, 128), pixel_data)
                
                if size == 8:  # Downscale to 8x8
                    img = img.resize((8, 8), Image.Resampling.LANCZOS)
                
                img.save(os.path.join(output_dir, f"{char_name}.png"), 'PNG')
    else:  # Old format (8x8)
        data = bytearray(data)
        for i in range(len(data) // 8):
            char_name = namedpart.get(str(i))
            if char_name:
                bitmap = np.zeros((8, 8, 4), dtype=np.uint8)
                for y in range(8):
                    byte = data[i * 8 + y]
                    for x in range(8):
                        pixel = (byte >> (7 - x)) & 1
                        if pixel:
                            bitmap[y, x] = [255, 255, 255, 255]
                        else:
                            bitmap[y, x] = [0, 0, 0, 0]
                
                img = Image.fromarray(bitmap, 'RGBA')
                if size == 128:  # Upscale to 128x128
                    img = img.resize((128, 128), Image.NEAREST)
                
                img.save(os.path.join(output_dir, f"{char_name}.png"), 'PNG')

def create_chr_file(input_dir, output_file, classic_mode=False):
    """
    Create a new willy.chr file from PNG images.
    Supports both 8x8 (classic) and 128x128 (HD) formats.
    
    Args:
        input_dir: Directory containing PNG files
        output_file: Output chr file path
        classic_mode: If True, create old format 8x8 chr file
    """
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
    for i in range(1, 41):
        namedpart_reverse[f"PIPE{i}"] = 50 + i
    namedpart_reverse["BALLPIT"] = 126
    namedpart_reverse["EMPTY"] = 127

    if classic_mode:
        # Create old format 8x8 chr file
        with open(output_file, 'wb') as f:
            max_char_index = max(namedpart_reverse.values()) + 1
            for char_index in range(max_char_index):
                char_name = None
                for name, idx in namedpart_reverse.items():
                    if idx == char_index:
                        char_name = name
                        break
                
                if char_name and os.path.exists(os.path.join(input_dir, f"{char_name}.png")):
                    img = Image.open(os.path.join(input_dir, f"{char_name}.png"))
                    img = img.convert('RGBA')
                    if img.size != (8, 8):
                        img = img.resize((8, 8), Image.Resampling.LANCZOS)
                    
                    # Convert to 8x8 bitmap
                    bitmap = np.array(img)
                    char_data = bytearray(8)
                    for y in range(8):
                        byte = 0
                        for x in range(8):
                            if bitmap[y, x][3] > 127:  # Check alpha channel
                                byte |= (1 << (7 - x))
                        char_data[y] = byte
                    f.write(char_data)
                else:
                    f.write(bytes([0] * 8))
    else:
        # Create new format 128x128 chr file
        metadata = {
            "version": 2,
            "width": 128,
            "height": 128,
            "channels": 4,
            "characters": namedpart_reverse
        }
        
        header_bytes = json.dumps(metadata).encode('utf-8')
        header_size = len(header_bytes)
        header_size_bytes = header_size.to_bytes(4, byteorder='little')
        
        with open(output_file, 'wb') as f:
            f.write(header_size_bytes)
            f.write(header_bytes)
            
            max_char_index = max(namedpart_reverse.values()) + 1
            for char_index in range(max_char_index):
                char_name = None
                for name, idx in namedpart_reverse.items():
                    if idx == char_index:
                        char_name = name
                        break
                
                if char_name and os.path.exists(os.path.join(input_dir, f"{char_name}.png")):
                    img = Image.open(os.path.join(input_dir, f"{char_name}.png"))
                    img = img.convert('RGBA')
                    if img.size != (128, 128):
                        img = img.resize((128, 128), Image.NEAREST)
                    f.write(img.tobytes())
                else:
                    f.write(bytes([0] * (128 * 128 * 4)))

def read_chr_file(chr_file):
    """
    Read a chr file and return the metadata and image data.
    Supports both old (8x8) and new (128x128) formats.
    """
    with open(chr_file, 'rb') as f:
        try:
            header_size = int.from_bytes(f.read(4), byteorder='little')
            header_data = f.read(header_size)
            metadata = json.loads(header_data.decode('utf-8'))
            
            if metadata["version"] == 2:
                return metadata, f.read()
        except:
            f.seek(0)
            return None, f.read()

def main():
    parser = argparse.ArgumentParser(description='Willy the Worm HD Sprite Tool')
    parser.add_argument('command', choices=['extract', 'create', 'info'],
                      help='Command to execute')
    parser.add_argument('--input', help='Input chr file or directory')
    parser.add_argument('--output', help='Output directory or chr file')
    parser.add_argument('--size', type=int, choices=[8, 128], default=128,
                      help='Output size for extraction (8 or 128 pixels)')
    parser.add_argument('--classic', action='store_true',
                      help='Create classic 8x8 format chr file')
    
    args = parser.parse_args()
    
    if args.command == 'extract':
        if not args.input or not args.output:
            parser.error('extract command requires --input and --output arguments')
        extract_characters(args.input, args.output, args.size)
    
    elif args.command == 'create':
        if not args.input or not args.output:
            parser.error('create command requires --input and --output arguments')
        create_chr_file(args.input, args.output, args.classic)
    
    else:  # info
        print("\nAvailable characters:")
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
        for i in range(51, 91):
            namedpart[i] = f"PIPE{i-50}"
        namedpart[126] = "BALLPIT"
        namedpart[127] = "EMPTY"
        
        for idx, name in sorted(namedpart.items()):
            print(f"{idx}: {name}")

if __name__ == '__main__':
    main()
