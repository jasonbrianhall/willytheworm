from setuptools import setup, find_packages

setup(
    name='Willy_the_Worm',
    version='1.0',
    py_modules=['willy', 'edwilly'],
    packages=find_packages(include=['willy.py', 'edwilly.py']),
    include_package_data=True,

    install_requires=[
        'pygame',
        'pillow'
    ],

    entry_points={
        'console_scripts': [
            'willy=willy:main'
        ]
    },

    data_files=[
        ('games/willytheworm/data', ['levels.json', 'willy.chr']),
        ('games/willytheworm/audio', ['audio/bell.mp3',
'audio/boop.mp3',
'audio/jump.mp3',
'audio/ladder.mp3',
'audio/present.mp3',
'audio/tack.mp3'])
    ],

    author='Jason Brian Hall',
    author_email='jasonbrianhall@gmail.com',
    description='Willy the Worm is a sprite based side-scrolling retro game that plays on a single screen.  The idea is to ring the bell on each level while avoiding balls, tacks, and falling into holes.  This game is a clone of a 1985 video game written by Alan Farme and plays extremely similar to the original game.'
    license='MIT',
    keywords='game worm fun',
    url='http://example.com/'
)
