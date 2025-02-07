#!/usr/bin/env python

import json

# Map the hex codes to named parts
named_parts = {
    "0": "WILLY_RIGHT",
    "1": "WILLY_LEFT",
    "2": "PRESENT",
    "3": "LADDER",
    "4": "TACK",
    "5": "UPSPRING",
    "6": "SIDESPRING",
    "7": "BALL",
    "8": "BELL",
    "51": "PIPE1",
    "52": "PIPE2",
    "53": "PIPE3",
    "54": "PIPE4",
    "55": "PIPE5",
    "56": "PIPE6",
    "57": "PIPE7",
    "58": "PIPE8",
    "59": "PIPE9",
    "60": "PIPE10",
    "61": "PIPE11",
    "62": "PIPE12",
    "63": "PIPE13",
    "64": "PIPE14",
    "65": "PIPE15",
    "66": "PIPE16",
    "67": "PIPE17",
    "68": "PIPE18",
    "69": "PIPE19",
    "70": "PIPE20",
    "71": "PIPE21",
    "72": "PIPE22",
    "73": "PIPE23",
    "74": "PIPE24",
    "75": "PIPE25",
    "76": "PIPE26",
    "77": "PIPE27",
    "78": "PIPE28",
    "79": "PIPE29",
    "80": "PIPE30",
    "81": "PIPE31",
    "82": "PIPE32",
    "83": "PIPE33",
    "84": "PIPE34",
    "85": "PIPE35",
    "86": "PIPE36",
    "87": "PIPE37",
    "88": "PIPE38",
    "89": "PIPE39",
    "90": "PIPE40",
    "126": "BALLPIT",
    "127": "EMPTY"
}

# Open the input file and read its contents
with open('WILLY.DAT', 'rb') as f:
    data = f.read().decode("latin-1")

# Create a dictionary to store the level data
levels = {}

# Split the data into 30-row chunks and parse each row
for i in range(8):
    level = {}
    for k in range(24):
        column = {}
        for j in range(40):
            offset = i*960 + j*24 + k
            code = ord(data[offset])
            if code==32:
                code=127
            else:
                code-=128
            column[str(j)] = named_parts.get(str(code), 'UNKNOWN' + "-" + str(code))
        if column:
            level[str(k)] = column
    if level:
        levels[f'level{i+1}'] = level
        
# Write the level data to a JSON file
with open('levels.json', 'w') as f:
    json.dump(levels, f, indent=4)

