#!/usr/bin/env python

import pygame
import json
import traceback
import sys
import copy
from willy import game as willymaingame
import willy
import os

# Constants
# SCALER = 4
CHAR_WIDTH = 8
CHAR_HEIGHT = 8
SCREEN_WIDTH = 42
MAX_WIDTH = 40
SCREEN_HEIGHT = 26
MAX_HEIGHT = 25
# MAX_LEVELS = 32
screenfillred=0
screenfillgreen=0
screenfillblue=255


def getMaxLevels(pathtolevels):
    with open(pathtolevels, 'r') as file:
        # Load the data from the file using the json.load() function
        level_data = json.load(file)
        original_level = copy.deepcopy(level_data)
        # print("Length", len(original_level))
        MAX_LEVELS = 0
        for x in original_level:
            if not "PIT" in x and "level" in x:
                MAX_LEVELS += 1
    return MAX_LEVELS


def intro(screen):
    screen.fill((0, 0, 255))
    exit = False
    while exit == False:
        datasize = 0
        # font_size = 32
        # fontdata = pygame.font.SysFont("Courier", font_size)
        # Render the text as a surface

        textdata = [
            ["WILLY_LEFT", " Willy the Worm ", "WILLY_RIGHT"],
            [""],
            ["By Jason Hall"],
            ["(original version by Alan Farmer 1985)"],
            [""],
            ["This code is Free Open Source Software (FOSS)"],
            ["Please feel free to do with it whatever you wish."],
            [""],
            ["If you do make changes though such as new levels,"],
            ["please share them with the world."],
            [""],
            [""],
            ["Welcome to the Willy the Worm ", "WILLY_RIGHT", "WILLY_RIGHT", " Editor."],
            [""],
            ["Left click places items, ", "PRESENT", " right click removes items"],
            ["Middle scroll button ", "LADDER", " scrolls between items."],
            ["F11 toggles full screen ", "BELL"],
            ["The 'S' Key saves the Level"],
            ["The 'L' Key Changes Level; The 'N' Key creates a new empty level"],
            ["The 'Q' Key or ESC exits the editor (without saving)"],
            ["The 'P' Key Tests the Level"],
            [""],
            ["You can also specify the level at the command line you wish to edit"],
            [""],
            ["The LAST BallPit ", "BALLPIT", " placed is where the balls come out."],
            [""],
            ["Good luck and have fun building levels!!!"],
            [""],
            ["Press Enter to Continue"],
        ]
        # screenwidth=SCREEN_WIDTH * CHAR_WIDTH * SCALER

        display_info = pygame.display.Info()
        screen_width = display_info.current_w
        screen_height = display_info.current_h
        font_size = int(screen_height / len(textdata))
        # font_size=font_size-font_size%8
        SCALER = int(font_size / 8)
        willyfont = willy.loadFont(SCALER)

        # screen = pygame.display.set_mode((SCREEN_WIDTH * CHAR_WIDTH * SCALER, SCREEN_HEIGHT * CHAR_HEIGHT * SCALER), pygame.FULLSCREEN)

        screenwidth = screen_width

        # font_size = 8*SCALER
        fontdata = pygame.font.SysFont("Courier", font_size)
        # Render the text as a surface
        counter = 0
        namer = 0
        screen.fill((0, 0, 255))
        for message in textdata:
            max_width = 0
            currentpos = 0
            for message2 in message:
                if not willyfont.get(message2) == None:
                    max_width += 8 * SCALER
                else:
                    text_surface = fontdata.render(message2, False, (255, 255, 255))
                    text_rect = text_surface.get_rect()
                    max_width += text_rect.width

            for message2 in message:
                if not willyfont.get(message2) == None:
                    text_surface = willyfont[message2]
                    # text_rect = text_surface.get_rect()
                    if currentpos == 0:
                        currentpos = (screenwidth - max_width) // 2
                    screen.blit(text_surface, (currentpos, font_size * counter + 2))
                    currentpos += 8 * SCALER
                else:
                    text_surface = fontdata.render(message2, False, (255, 255, 255))
                    text_rect = text_surface.get_rect()
                    if currentpos == 0:
                        currentpos = (screenwidth - max_width) // 2
                    screen.blit(text_surface, (currentpos, font_size * counter + 2))
                    currentpos += text_rect.width

            counter += 1

        # Calculate the number of characters that fit horizontally and vertically in the window

        pygame.display.flip()
        for event in pygame.event.get():
            # Keyboard Events
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_RETURN:
                    exit = True
                if event.key == pygame.K_F11:
                    pygame.display.toggle_fullscreen()
                    screen.fill((0, 0, 255))

                if event.key == pygame.K_ESCAPE:
                    print("Goodbye.	 Thank you for playing Willy the Worm!!!")
                    sys.exit(0)

def main():
    pygame.init()
    display_info = pygame.display.Info()
    screen_width = display_info.current_w
    screen_height = display_info.current_h
    # Keep current resolution but use the smallest scaler
    SCALER1 = int(screen_width / (SCREEN_WIDTH * CHAR_WIDTH))
    SCALER2 = int(screen_height / (SCREEN_HEIGHT * CHAR_HEIGHT))
    if SCALER1 <= SCALER2:
        SCALER = SCALER1
    else:
        SCALER = SCALER2
    # screen = pygame.display.set_mode((SCREEN_WIDTH * CHAR_WIDTH * SCALER, SCREEN_HEIGHT * CHAR_HEIGHT * SCALER), pygame.FULLSCREEN)
    screen = pygame.display.set_mode((screen_width, screen_height), pygame.FULLSCREEN)
    pygame.display.set_caption('Willy the Worm Editor')

    levelFile = "levels.json"
    helpmessage = sys.argv[0] + " -L levelsFile -h (HELP) --help (help)"

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "-L" and i + 1 < len(sys.argv):
            try:
                levelFile = sys.argv[i + 1]
            except ValueError:
                print("Invalid argument for -L")
                sys.exit(1)
            i += 1
        elif arg == "-h" or arg == "--help":
            print(helpmessage)
            sys.exit(1)
        else:
            print("Unknown argument:", arg)
            print(helpmessage)
            sys.exit(1)
        i += 1

    intro(screen)
    game(screen, SCALER, levelFile=levelFile)


def game(screen, SCALER, levelFile="levels.json"):
    # global SCALER
    if len(sys.argv) != 2:
        level = 1
    else:
        try:
            level = int(sys.argv[1])
        except:
            level = 1

    if getattr(sys, 'frozen', False):
        __file__ = os.path.dirname(sys.executable)
    else:
        __file__ = "."
    bundle_dir = getattr(sys, '_MEIPASS', os.path.abspath(os.path.dirname(__file__)))

    path_to_levels = os.path.abspath(os.path.join(bundle_dir, levelFile))

    if not os.path.isfile(path_to_levels):
        path_to_levels = levelFile

    MAX_LEVELS = getMaxLevels(path_to_levels)

    if level > 0 and level <= MAX_LEVELS:
        currentlevel = "level" + str(level)
    else:
        currentlevel = "level1"

    # Initialize Pygame

    global screenfillred
    global screenfillblue
    global screenfillgreen

    font_size = 32
    fontdata = pygame.font.SysFont("Courier", font_size)

    # Load the font
    font = willy.loadFont(SCALER, screenfillred, screenfillgreen, screenfillblue)
    del font["BALL"]
    # Create a 2D array to store the level data
    # level_data = [[None] * SCREEN_WIDTH for i in range(SCREEN_HEIGHT)]
    # Create a 2D array to store the level data
    # level_data = {}
    # level_data[currentlevel]={}

    try:
        with open(levelFile, 'r') as file:
            # Load the data from the file using the json.load() function
            level_data = json.load(file)
    except:
        traceback.print_exc()
        print("Can't load", levelFile, "; starting over")
        level_data = {}
        level_data[currentlevel] = {}

    iterator = iter(font.items())
    currentitem = next(iterator)

    # Game loop
    running = True

    row = 0
    col = SCREEN_WIDTH - 1
    # level_data[row][col] = font["WILLY_RIGHT"]
    if level_data.get(currentlevel) == None:
        # level_data[curentlevel]={}
        level_data[currentlevel] = {}
    for row in range(SCREEN_HEIGHT):
        if level_data.get(currentlevel).get(str(row)) == None:
            level_data[currentlevel][str(row)] = {}
        for col in range(SCREEN_WIDTH):
            if level_data[currentlevel].get(str(row)).get(str(col)) == None:
                level_data[currentlevel][str(row)][str(col)] = "EMPTY"

    while running:
        # Handle events
        for event in pygame.event.get():
            # Close Event
            if event.type == pygame.QUIT:
                running = False
            # Keyboard Events
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_F11:
                    pygame.display.toggle_fullscreen()
                elif event.key == pygame.K_n:
                    MAX_LEVELS += 1
                elif event.key == pygame.K_l:
                    level += 1
                    level = level % (MAX_LEVELS + 1)
                    if level == 0:
                        level = 1
                    currentlevel = "level" + str(level)
                    if level_data.get(currentlevel) == None:
                        level_data[currentlevel] = {}
                    for row in range(SCREEN_HEIGHT):
                        if level_data.get(currentlevel).get(str(row)) == None:
                            level_data[currentlevel][str(row)] = {}
                        for col in range(SCREEN_WIDTH):
                            if level_data[currentlevel].get(str(row)).get(str(col)) == None:
                                level_data[currentlevel][str(row)][str(col)] = "EMPTY"
                elif event.key == pygame.K_s:
                    # data=json.dumps(level_data)
                    new_dict = copy.deepcopy(level_data)
                    for x in level_data:
                        try:
                            del new_dict[x]["0"][str(SCREEN_WIDTH - 1)]
                        except:
                            pass
                        for y in level_data[x]:
                            for z in level_data[x][y]:
                                try:
                                    if level_data[x][y][z] == "EMPTY":
                                        try:
                                            del new_dict[x][y][z]
                                        except:
                                            pass

                                except:
                                    pass
                    with open(levelFile, 'w') as writefile:
                        # Write the data to the file using the json.dump() function
                        json.dump(new_dict, writefile, indent=4)
                elif event.key == pygame.K_q or event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_p:
                    willymaingame(screen, currentlevel, level, SCALER, numberoflives=1, levelFile=levelFile)
                    # Stop Hiding the mouse cursor if it's hidden
                    pygame.mouse.set_visible(True)

                    # Stop capturing the mouse input
                    pygame.event.set_grab(False)
                elif event.key == pygame.K_F5:
                    if screenfillred == 255:
                        screenfillred = 0
                    elif screenfillred >= 0 and screenfillred < 192:
                        screenfillred += 64
                    else:
                        screenfillred = 255
                    font = willy.loadFont(SCALER, screenfillred, screenfillgreen, screenfillblue)
                    del font["BALL"]

                elif event.key == pygame.K_F6:
                    if screenfillgreen == 255:
                        screenfillgreen = 0
                    elif screenfillgreen >= 0 and screenfillgreen < 192:
                        screenfillgreen += 64
                    else:
                        screenfillgreen = 255
                    font = willy.loadFont(SCALER, screenfillred, screenfillgreen, screenfillblue)
                    del font["BALL"]
                elif event.key == pygame.K_F7:
                    if screenfillblue == 255:
                        screenfillblue = 0
                    elif screenfillblue >= 0 and screenfillblue < 192:
                        screenfillblue += 64
                    else:
                        screenfillblue = 255
                    font = willy.loadFont(SCALER, screenfillred, screenfillgreen, screenfillblue)
                    del font["BALL"]                    
                elif event.key == pygame.K_F8:
                    screenfillblue=255
                    screenfillred=0
                    screenfillgreen=0
                    font = willy.loadFont(SCALER, screenfillred, screenfillgreen, screenfillblue)
                    del font["BALL"]                   
            # Right Button Deletes Object
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 3:
                # Get the mouse position
                mouse_pos = pygame.mouse.get_pos()
                # Calculate the row and column in the level data array
                row = mouse_pos[1] // (CHAR_HEIGHT * SCALER)
                col = mouse_pos[0] // (CHAR_WIDTH * SCALER)
                # Set the character image in the level data array
                if col < MAX_WIDTH and row < MAX_HEIGHT:
                    try:
                        level_data[currentlevel][str(row)][str(col)] = "EMPTY"
                        level_data[currentlevel][str(row)][str(col)] = "EMPTY"
                    except:
                        traceback.print_exc()
                        pass
            # Left Click Places Object
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                # Get the mouse position
                mouse_pos = pygame.mouse.get_pos()
                # Calculate the row and column in the level data array
                row = mouse_pos[1] // (CHAR_HEIGHT * SCALER)
                col = mouse_pos[0] // (CHAR_WIDTH * SCALER)

                if col < MAX_WIDTH and row < MAX_HEIGHT:
                    try:
                        level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                    except:
                        level_data[currentlevel][str(row)] = {}
                        level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                        pass
                    # Deleting extra Willies
                    if currentitem[0] == "WILLY_RIGHT" or currentitem[0] == "WILLY_LEFT":
                        for newrow in level_data[currentlevel]:
                            for newcol in level_data[currentlevel][newrow]:
                                if (
                                    level_data[currentlevel][newrow][newcol] == "WILLY_RIGHT"
                                    or level_data[currentlevel][newrow][newcol] == "WILLY_LEFT"
                                ):
                                    if not (newcol == str(col) and newrow == str(row)):
                                        level_data[currentlevel][newrow][newcol] = "EMPTY"
                # Last ballpit added is where the balls come out of
                if currentitem[0] == "BALLPIT":
                    level_data[currentlevel + "PIT"] = {}
                    level_data[currentlevel + "PIT"]["PRIMARYBALLPIT"] = (row, col)
            # Wheel Button Down
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 5:
                # Get the mouse position
                mouse_pos = pygame.mouse.get_pos()
                # Calculate the row and column in the level data array
                row = 0
                col = SCREEN_WIDTH - 1
                # Set the character image in the level data array
                try:
                    currentitem = next(iterator)
                # end of list
                except:
                    iterator = iter(font.items())
                    currentitem = next(iterator)
                    pass
                try:
                    level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                except:
                    level_data[currentlevel][str(row)] = {}
                    level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                    pass

            # Wheel Button UP
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 4:
                # Get the mouse position
                mouse_pos = pygame.mouse.get_pos()
                # Calculate the row and column in the level data array
                row = 0
                col = SCREEN_WIDTH - 1
                # Set the character image in the level data array
                # currentitem=iterator
                temp = currentitem
                iterator = iter(font.items())
                currentitem = next(iterator)
                previous = currentitem
                exittheloop = False
                while exittheloop == False:
                    if currentitem == temp:
                        exittheloop = True
                    else:
                        previous = currentitem
                        currentitem = next(iterator)
                currentitem = previous
                try:
                    level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                except:
                    level_data[currentlevel][str(row)] = {}
                    level_data[currentlevel][str(row)][str(col)] = currentitem[0]
                    pass

        # Clear the screen
        # screen.fill((0, 0, 0))

        # Draw the level data
        '''for row in range(SCREEN_HEIGHT):
			for col in range(SCREEN_WIDTH):
				try:
					char_img = font[level_data[row][col]]
				except:
					traceback.print_exc()
					char_img = font["EMPTY"]
					pass
				if char_img is not None:
					# Draw the character image
					screen.blit(char_img, (col * CHAR_WIDTH * SCALER, row * CHAR_HEIGHT * SCALER))'''
        screen.fill((screenfillred, screenfillgreen, screenfillblue))

        for row in level_data[currentlevel]:
            for col in level_data[currentlevel][row]:
                # This will error on primary ball pit
                try:
                    char_img = font[level_data[currentlevel][row][col]]
                    screen.blit(char_img, (int(col) * CHAR_WIDTH * SCALER, int(row) * CHAR_HEIGHT * SCALER))
                except:
                    pass
                # print(char_img)

        # Render the text as a surface
        text = "Level: " + str(level)
        text_surface = fontdata.render(text, True, (255, 255, 255))

        text_x = 6 * 25 * SCALER
        text_y = (SCREEN_HEIGHT * CHAR_HEIGHT * SCALER) - font_size
        screen.blit(text_surface, (text_x, text_y))

        # Update the screen
        pygame.display.flip()

    # Clean up
    pygame.quit()


if __name__ == '__main__':
    main()
