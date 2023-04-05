#!/bin/bash

import pygame
from PIL import Image
import json
import traceback

# Constants
SCALER = 4
CHAR_WIDTH = 8
CHAR_HEIGHT = 8
SCREEN_WIDTH = 42
MAX_WIDTH = 40
SCREEN_HEIGHT = 24

def loadFont():

	namedpart={}
	namedpart["0"]="WILLY_RIGHT"
	#namedpart["1"]="WILLY_LEFT"
	namedpart["2"]="PRESENT"
	namedpart["3"]="LADDER"
	namedpart["4"]="TACK"
	namedpart["5"]="UPSPRING"
	namedpart["6"]="SIDESPRING"
	namedpart["7"]="BALL"
	namedpart["8"]="BELL"
	namedpart["51"]="PIPE1"
	namedpart["52"]="PIPE2"
	namedpart["53"]="PIPE3"
	namedpart["54"]="PIPE4"
	namedpart["55"]="PIPE5"
	namedpart["56"]="PIPE6"
	namedpart["57"]="PIPE7"
	namedpart["58"]="PIPE8"
	namedpart["59"]="PIPE9"
	namedpart["60"]="PIPE10"
	namedpart["61"]="PIPE11"
	namedpart["62"]="PIPE12"
	namedpart["63"]="PIPE13"
	namedpart["64"]="PIPE14"
	namedpart["65"]="PIPE15"
	namedpart["66"]="PIPE16"
	namedpart["67"]="PIPE17"
	namedpart["68"]="PIPE18"
	namedpart["69"]="PIPE19"
	namedpart["70"]="PIPE20"
	namedpart["71"]="PIPE21"
	namedpart["72"]="PIPE22"
	namedpart["73"]="PIPE23"
	namedpart["74"]="PIPE24"
	namedpart["75"]="PIPE25"
	namedpart["76"]="PIPE26"
	namedpart["77"]="PIPE27"
	namedpart["78"]="PIPE28"
	namedpart["79"]="PIPE29"
	namedpart["80"]="PIPE30"
	namedpart["81"]="PIPE31"
	namedpart["82"]="PIPE32"
	namedpart["83"]="PIPE33"
	namedpart["84"]="PIPE34"
	namedpart["85"]="PIPE35"
	namedpart["86"]="PIPE36"
	namedpart["87"]="PIPE37"
	namedpart["88"]="PIPE38"
	namedpart["89"]="PIPE39"
	namedpart["90"]="PIPE40"
	namedpart["126"]="BALLPIT"
	namedpart["127"]="EMPTY"
	

	# Define the colors (in RGB format)
	BLUE = (0, 0, 255)
	WHITE = (255, 255, 255)

	# Define the size of the output image (in pixels)
	IMAGE_WIDTH = 128
	IMAGE_HEIGHT = 256

	# Open the WILLY.CHR file
	with open('WILLY.CHR', 'rb') as f:
		# Read the file contents into a bytearray
		data = bytearray(f.read())

	# Create a new PIL image
	img = Image.new('RGB', (IMAGE_WIDTH, IMAGE_HEIGHT), BLUE)

	char_array={}

	counter=0
	# Loop through the characters in the file
	for i in range(len(data) // 8):
		# Extract the bits for each row of the character
		bits = [((data[i * 8 + j] >> k) & 1) for j in range(8) for k in range(7, -1, -1)]
		
		# Create a new PIL image for the character
		char_img = Image.new('RGB', (CHAR_WIDTH, CHAR_HEIGHT), BLUE)
		# Loop through the rows of the character
		for y in range(CHAR_HEIGHT):
			# Loop through the pixels in the row
			for x in range(CHAR_WIDTH):
				# Calculate the index of the pixel in the bits array
				index = y * CHAR_WIDTH + x
				
				# If the bit is set, set the pixel to white
				if bits[index] == 1:
					char_img.putpixel((x, y), WHITE)

		new_size = (char_img.size[0] * SCALER, char_img.size[1] * SCALER)
		char_img = char_img.resize(new_size)
		pygame_image = pygame.image.fromstring(char_img.tobytes(), char_img.size, char_img.mode).convert()
		try:
			partnumber=namedpart[str(counter)]
			char_array[partnumber]=pygame_image
		except:
			#char_array[str(counter)]=pygame_image
			pass
		counter+=1

	return char_array

def main():
	# Initialize Pygame
	pygame.init()
	screen = pygame.display.set_mode((SCREEN_WIDTH * CHAR_WIDTH * SCALER, SCREEN_HEIGHT * CHAR_HEIGHT * SCALER))

	# Load the font
	font = loadFont()


	# Create a 2D array to store the level data
	#level_data = [[None] * SCREEN_WIDTH for i in range(SCREEN_HEIGHT)]
	# Create a 2D array to store the level data
	#level_data2 = {}
	#level_data2[currentlevel]={}

	currentlevel="level1"

	try:
		with open('levels.json', 'r') as file:
			# Load the data from the file using the json.load() function
			level_data2 = json.load(file)
			level_data = [[None] * SCREEN_WIDTH for i in range(SCREEN_HEIGHT)]
			for col in range(0, SCREEN_WIDTH):
				for row in range(0, SCREEN_HEIGHT):
					level_data[row][col] = font["EMPTY"]

			for level in level_data2:
				for col in level_data2[currentlevel]:
					for row in level_data2[currentlevel][col]:
						data=level_data2[currentlevel][col][row]
						print(data)
						try:
							level_data[int(col)][int(row)]=font[data]
						except:
							traceback.print_exc()
							pass
	except:
		print("Can't load levels.json; starting over")
		level_data = [[None] * SCREEN_WIDTH for i in range(SCREEN_HEIGHT)]
		level_data2 = {}
		level_data2[currentlevel]={}
		for col in range(0, SCREEN_WIDTH):
			for row in range(0, SCREEN_HEIGHT):
				level_data[row][col] = font["EMPTY"]


	#print(level_data)


	iterator = iter(font.items())
	currentitem=next(iterator)

	# Game loop
	running = True

	row = 0
	col = SCREEN_WIDTH-1
	level_data[row][col] = font["WILLY_RIGHT"]

	while running:
		# Handle events
		for event in pygame.event.get():
			# Close Event
			if event.type == pygame.QUIT:
				running = False
			# Keyboard Events
			elif event.type == pygame.KEYDOWN:
				if event.key == pygame.K_s:
					#data=json.dumps(level_data2)
					with open('levels.json', 'w') as writefile:
						# Write the data to the file using the json.dump() function
						json.dump(level_data2, writefile)
			# Right Button Deletes Object
			elif event.type == pygame.MOUSEBUTTONDOWN  and event.button==3:
				# Get the mouse position
				mouse_pos = pygame.mouse.get_pos()
				# Calculate the row and column in the level data array
				row = mouse_pos[1] // (CHAR_HEIGHT * SCALER)
				col = mouse_pos[0] // (CHAR_WIDTH * SCALER)
				# Set the character image in the level data array
				if col<MAX_WIDTH:
					level_data[row][col] = font["EMPTY"]
					try:
						del level_data2[currentlevel][row][col]
						if len(level_data2[currentlevel][row])==0:
							 del level_data2[currentlevel][row]
					except:
						pass
			# Left Click Places Object
			elif event.type == pygame.MOUSEBUTTONDOWN and event.button==1:
				# Get the mouse position
				mouse_pos = pygame.mouse.get_pos()
				# Calculate the row and column in the level data array
				row = mouse_pos[1] // (CHAR_HEIGHT * SCALER)
				col = mouse_pos[0] // (CHAR_WIDTH * SCALER)
					 
				if col<MAX_WIDTH:
					level_data[row][col] = font[currentitem[0]]
					try:
						level_data2[currentlevel][row][col] = currentitem[0]
					except:
						level_data2[currentlevel][row] = {}
						level_data2[currentlevel][row][col] = currentitem[0]
						pass
					
			# Wheel Button Down
			elif event.type == pygame.MOUSEBUTTONDOWN and event.button==5:
				# Get the mouse position
				mouse_pos = pygame.mouse.get_pos()
				# Calculate the row and column in the level data array
				row = 0
				col = SCREEN_WIDTH-1
				# Set the character image in the level data array
				try:
					currentitem=next(iterator)
				#end of list
				except:
					iterator = iter(font.items())
					currentitem=next(iterator)
					pass
				level_data[row][col] = font[currentitem[0]]
				try:
					level_data2[currentlevel][row][col] = currentitem[0]
				except:
					level_data2[currentlevel][row] = {}
					level_data2[currentlevel][row][col] = currentitem[0]
					pass

			# Wheel Button UP
			elif event.type == pygame.MOUSEBUTTONDOWN and event.button==4:
				# Get the mouse position
				mouse_pos = pygame.mouse.get_pos()
				# Calculate the row and column in the level data array
				row = 0
				col = SCREEN_WIDTH-1
				# Set the character image in the level data array
				#currentitem=iterator
				temp=currentitem
				iterator = iter(font.items())
				currentitem=next(iterator)
				previous=currentitem
				exittheloop=False
				while exittheloop==False:
					if currentitem==temp:
						 exittheloop=True
					else:
						 previous=currentitem
						 currentitem=next(iterator)
				currentitem=previous
				level_data[row][col] = font[currentitem[0]]
				try:
					level_data2[currentlevel][row][col] = currentitem[0]
				except:
					level_data2[currentlevel][row] = {}
					level_data2[currentlevel][row][col] = currentitem[0]
					pass



		# Clear the screen
		screen.fill((0, 0, 0))

		# Draw the level data
		for row in range(SCREEN_HEIGHT):
			for col in range(SCREEN_WIDTH):
				char_img = level_data[row][col]
				if char_img is not None:
					# Draw the character image
					screen.blit(char_img, (col * CHAR_WIDTH * SCALER, row * CHAR_HEIGHT * SCALER))

		# Update the screen
		pygame.display.flip()

	# Clean up
	pygame.quit()
	
if __name__ == '__main__':
    main()

