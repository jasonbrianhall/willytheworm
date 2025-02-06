#!/usr/bin/env python3
import sys
import os
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                           QAction, QMenuBar, QMessageBox, QFileDialog)
from PyQt5.QtCore import Qt, QTimer
import pygame

# Import specific functions from willy
from willy import (loadFont, SCREEN_WIDTH, SCREEN_HEIGHT, 
                  CHAR_WIDTH, CHAR_HEIGHT, screenfillred, screenfillgreen, screenfillblue)

class PygameWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_OpaquePaintEvent)
        self.setAttribute(Qt.WA_NoSystemBackground)
        self.setFocusPolicy(Qt.StrongFocus)
        
        # Initialize Pygame
        self.scaler = 4  # Default scaler
        self.init_size()
        
        # Game state
        self.waiting_for_enter = True
        self.game_state = "INIT"  # INIT, INTRO, PLAYING, SCORE
        self.screen = None
        self.pygame_initialized = False
        
        # Setup update timer
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update_game)
        self.timer.start(16)  # ~60 FPS
    
    def init_size(self):
        """Calculate window size based on game dimensions"""
        self.game_width = SCREEN_WIDTH * CHAR_WIDTH * self.scaler
        self.game_height = SCREEN_HEIGHT * CHAR_HEIGHT * self.scaler
        self.setMinimumSize(self.game_width, self.game_height)

    def initializeScreen(self):
        """Initialize or reinitialize the Pygame screen"""
        if hasattr(self, 'game_width'):
            os.environ['SDL_WINDOWID'] = str(int(self.winId()))
            pygame.init()
            self.screen = pygame.display.set_mode((self.game_width, self.game_height))
            pygame.display.init()
            self.screen.fill((screenfillred, screenfillgreen, screenfillblue))
            pygame.display.flip()
            self.pygame_initialized = True
            return self.screen

    def update_game(self):
        """Update game state and process events"""
        if not hasattr(self, 'screen') or not self.screen:
            self.screen = self.initializeScreen()
            return

        # Let Qt process its events
        QApplication.processEvents()
            
        # Process Pygame events
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.parent().close()
            elif event.type == pygame.KEYDOWN:
                if self.waiting_for_enter and event.key == pygame.K_RETURN:
                    self.waiting_for_enter = False
                    self.start_main_game()
                    
        # Keep display updated
        pygame.display.flip()

    def start_main_game(self):
        """Start the actual game"""
        from willy import game, intro, game_score
        try:
            # Show intro screen
            intro(self.screen)
            
            # Start main game loop
            currentlevel = f"level{self.parent().game_args['level']}"
            score = game(
                self.screen,
                currentlevel,
                self.parent().game_args['level'],
                self.parent().game_args['scaler'],
                self.parent().game_args['wasd'],
                self.parent().game_args['flash'],
                self.parent().game_args['numberofballs'],
                self.parent().game_args['mousesupport'],
                self.parent().game_args['fps'],
                numberoflives=5,
                levelFile=self.parent().game_args['levelFile']
            )
            game_score(self.screen, score)
            self.parent().game_args['level'] = 1
            self.waiting_for_enter = True
            
        except Exception as e:
            QMessageBox.critical(self.parent(), "Error", f"Game error: {str(e)}")

    def keyPressEvent(self, event):
        """Handle Qt key events"""
        key_map = {
            Qt.Key_Return: pygame.K_RETURN,
            Qt.Key_Enter: pygame.K_RETURN,
            Qt.Key_Space: pygame.K_SPACE,
            Qt.Key_Left: pygame.K_LEFT,
            Qt.Key_Right: pygame.K_RIGHT,
            Qt.Key_Up: pygame.K_UP,
            Qt.Key_Down: pygame.K_DOWN,
            Qt.Key_Escape: pygame.K_ESCAPE,
            Qt.Key_W: pygame.K_w,
            Qt.Key_A: pygame.K_a,
            Qt.Key_S: pygame.K_s,
            Qt.Key_D: pygame.K_d,
            Qt.Key_F11: pygame.K_F11
        }
        
        if event.key() in key_map:
            pygame_event = pygame.event.Event(pygame.KEYDOWN, {'key': key_map[event.key()]})
            pygame.event.post(pygame_event)
        
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event):
        """Handle Qt key release events"""
        key_map = {
            Qt.Key_Return: pygame.K_RETURN,
            Qt.Key_Enter: pygame.K_RETURN,
            Qt.Key_Space: pygame.K_SPACE,
            Qt.Key_Left: pygame.K_LEFT,
            Qt.Key_Right: pygame.K_RIGHT,
            Qt.Key_Up: pygame.K_UP,
            Qt.Key_Down: pygame.K_DOWN,
            Qt.Key_Escape: pygame.K_ESCAPE,
            Qt.Key_W: pygame.K_w,
            Qt.Key_A: pygame.K_a,
            Qt.Key_S: pygame.K_s,
            Qt.Key_D: pygame.K_d,
            Qt.Key_F11: pygame.K_F11
        }
        
        if event.key() in key_map:
            pygame_event = pygame.event.Event(pygame.KEYUP, {'key': key_map[event.key()]})
            pygame.event.post(pygame_event)
            
        super().keyReleaseEvent(event)

class WillyWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.game_args = {
            'level': 1,
            'wasd': False,
            'flash': True,
            'numberofballs': 9,
            'mousesupport': False,
            'fps': 10,
            'levelFile': "levels.json",
            'scaler': 4
        }
        self.initUI()
        
    def initUI(self):
        # Create central widget
        self.pygame_widget = PygameWidget(self)
        self.setCentralWidget(self.pygame_widget)
        
        # Create menu bar
        menubar = self.menuBar()
        
        # File menu
        fileMenu = menubar.addMenu('&File')
        
        newGameAction = QAction('&New Game', self)
        newGameAction.setShortcut('Ctrl+N')
        newGameAction.triggered.connect(self.new_game)
        fileMenu.addAction(newGameAction)
        
        fileMenu.addSeparator()
        
        exitAction = QAction('E&xit', self)
        exitAction.setShortcut('Ctrl+Q')
        exitAction.triggered.connect(self.close)
        fileMenu.addAction(exitAction)
        
        # Options menu
        optionsMenu = menubar.addMenu('&Options')
        
        soundAction = QAction('&Toggle Sound', self)
        soundAction.setCheckable(True)
        soundAction.setChecked(True)
        soundAction.triggered.connect(self.toggle_sound)
        optionsMenu.addAction(soundAction)
        
        wasdAction = QAction('&WASD Controls', self)
        wasdAction.setCheckable(True)
        wasdAction.setChecked(False)
        wasdAction.triggered.connect(self.toggle_wasd)
        optionsMenu.addAction(wasdAction)
        
        mouseAction = QAction('Enable &Mouse', self)
        mouseAction.setCheckable(True)
        mouseAction.setChecked(False)
        mouseAction.triggered.connect(self.toggle_mouse)
        optionsMenu.addAction(mouseAction)
        
        # Help menu
        helpMenu = menubar.addMenu('&Help')
        
        aboutAction = QAction('&About', self)
        aboutAction.triggered.connect(self.show_about)
        helpMenu.addAction(aboutAction)
        
        # Set window properties
        self.setWindowTitle('Willy the Worm')
        
        # Calculate window size including menu bar
        total_height = self.pygame_widget.game_height + menubar.height()
        self.setFixedSize(self.pygame_widget.game_width, total_height)
        
    def new_game(self):
        """Start a new game"""
        reply = QMessageBox.question(self, 'New Game', 
                                   'Are you sure you want to start a new game?',
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        
        if reply == QMessageBox.Yes:
            self.game_args['level'] = 1
            self.pygame_widget.waiting_for_enter = True
            self.pygame_widget.start_main_game()
    
    def toggle_sound(self, checked):
        """Toggle sound on/off"""
        global soundenabled
        soundenabled = checked
        
    def toggle_wasd(self, checked):
        """Toggle between WASD and arrow key controls"""
        self.game_args['wasd'] = checked
        
    def toggle_mouse(self, checked):
        """Toggle mouse support"""
        self.game_args['mousesupport'] = checked
    
    def show_about(self):
        """Show about dialog"""
        QMessageBox.about(self, "About Willy the Worm",
                         "Willy the Worm\n\n"
                         "A classic game by Alan Farmer (1985)\n"
                         "Python version by Jason Hall\n"
                         "PyQt5 interface added 2025")
        
    def closeEvent(self, event):
        """Handle window close event"""
        reply = QMessageBox.question(self, 'Exit',
                                   'Are you sure you want to exit?',
                                   QMessageBox.Yes | QMessageBox.No,
                                   QMessageBox.No)
        
        if reply == QMessageBox.Yes:
            pygame.quit()
            event.accept()
        else:
            event.ignore()

def main():
    # Enable high DPI scaling
    if hasattr(Qt, 'AA_EnableHighDpiScaling'):
        QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    if hasattr(Qt, 'AA_UseHighDpiPixmaps'):
        QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
        
    app = QApplication(sys.argv)
    
    # Force software rendering for Pygame
    os.environ["SDL_VIDEODRIVER"] = "x11"
    
    window = WillyWindow()
    window.show()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
