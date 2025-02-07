#!/usr/bin/env python3
import sys
import os
import json
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QAction, 
                            QMenuBar, QMessageBox, QHBoxLayout, QVBoxLayout, QLabel)
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont
import pygame

from willy import (loadFont, SCREEN_WIDTH, SCREEN_HEIGHT, CHAR_WIDTH, CHAR_HEIGHT, 
                  screenfillred, screenfillgreen, screenfillblue, game, intro, game_score)

class WillyGame:
    def __init__(self):
        pygame.init()
        self.game_width = SCREEN_WIDTH * CHAR_WIDTH * 4
        self.game_height = SCREEN_HEIGHT * CHAR_HEIGHT * 4
        self.screen = pygame.display.set_mode((self.game_width, self.game_height))
        self.waiting_for_enter = True
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

    def loop(self, window):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return True
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_LALT and self.game_args['mousesupport']:
                    pygame.event.set_grab(True)
                    pygame.mouse.set_visible(False)
                elif self.waiting_for_enter and event.key == pygame.K_RETURN:
                    self.waiting_for_enter = False
                    self.start_game()
                    
        pygame.display.flip()
        return False
        
    def start_game(self):
        try:
            intro(self.screen)
            currentlevel = f"level{self.game_args['level']}"
            score = game(
                self.screen,
                currentlevel,
                self.game_args['level'],
                self.game_args['scaler'],
                self.game_args['wasd'],
                self.game_args['flash'],
                self.game_args['numberofballs'],
                self.game_args['mousesupport'],
                self.game_args['fps'],
                numberoflives=5,
                levelFile=self.game_args['levelFile']
            )
            game_score(self.screen, score)
            self.game_args['level'] = 1
            self.waiting_for_enter = True
        except Exception as e:
            print(f"Game error: {str(e)}")

class ScoreBoard(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.initUI()
        self.startTimer(1000)  # Update every second

    def initUI(self):
        layout = QVBoxLayout()
        self.scores_label = QLabel()
        self.scores_label.setAlignment(Qt.AlignLeft | Qt.AlignTop)
        self.scores_label.setFont(QFont('Courier', 12))
        layout.addWidget(self.scores_label)
        self.setLayout(layout)
        self.updateScores()

    def updateScores(self):
        try:
            home_dir = os.path.expanduser("~")
            score_file = os.path.join(home_dir, ".willytheworm", "willy.scr")
            
            with open(score_file, 'r') as f:
                scores = json.load(f)
                
            text = "All-time Nightcrawlers\n"
            text += "-" * 30 + "\n"
            for i, (name, score) in enumerate(scores['hiscoreP'][:5]):
                text += f"{i+1:2d}. {name:15s} {score:5d}\n"
                
            text += "\nToday's Best Pinworms\n"
            text += "-" * 30 + "\n"
            for i, (name, score) in enumerate(scores['hiscoreT'][:5]):
                text += f"{i+1:2d}. {name:15s} {score:5d}\n"
                
            self.scores_label.setText(text)
        except:
            self.scores_label.setText("No high scores yet!")

    def timerEvent(self, event):
        self.updateScores()

class WillyWindow(QMainWindow):
    def __init__(self, game):
        super().__init__()
        self.game = game
        self.init_ui()
        self.init_pygame()
        
    def init_ui(self):
        self.setWindowTitle('Willy the Worm')
        
        central_widget = QWidget()
        layout = QHBoxLayout()
        
        # Add scoreboard
        self.scoreboard = ScoreBoard()
        layout.addWidget(self.scoreboard)
        
        # Add Pygame surface
        pygame_widget = QWidget()
        pygame_widget.setAttribute(Qt.WA_OpaquePaintEvent)
        layout.addWidget(pygame_widget)
        
        central_widget.setLayout(layout)
        self.setCentralWidget(central_widget)
        
        self.init_menu()
        self.show()

    def init_menu(self):
        menubar = self.menuBar()
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
        
        helpMenu = menubar.addMenu('&Help')
        aboutAction = QAction('&About', self)
        aboutAction.triggered.connect(self.show_about)
        helpMenu.addAction(aboutAction)

    def init_pygame(self):
        self.timer = QTimer()
        self.timer.timeout.connect(self.pygame_loop)
        self.timer.start(16)  # ~60 FPS

    def pygame_loop(self):
        if self.game.loop(self):
            self.close()
            
    def new_game(self):
        reply = QMessageBox.question(self, 'New Game', 
                                   'Are you sure you want to start a new game?',
                                   QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            self.game.game_args['level'] = 1
            self.game.waiting_for_enter = True
            
    def toggle_sound(self, checked):
        from willy import soundenabled
        globals()['soundenabled'] = checked
        
    def toggle_wasd(self, checked):
        self.game.game_args['wasd'] = checked
        
    def toggle_mouse(self, checked):
        self.game.game_args['mousesupport'] = checked
    
    def show_about(self):
        QMessageBox.about(self, "About Willy the Worm",
                         "Willy the Worm\n\n"
                         "A classic game by Alan Farmer (1985)\n"
                         "Python version by Jason Hall\n"
                         "PyQt5 interface added 2025")
        
    def closeEvent(self, event):
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
    if hasattr(Qt, 'AA_EnableHighDpiScaling'):
        QApplication.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    if hasattr(Qt, 'AA_UseHighDpiPixmaps'):
        QApplication.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
    
    game = WillyGame()
    app = QApplication(sys.argv)
    window = WillyWindow(game)
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
