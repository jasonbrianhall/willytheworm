from setuptools import setup, find_packages

setup(
    name='willy',
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
        ('data', ['levels.json']),
        ('audio', ['audio/bell.mp3',
'audio/boop.mp3',
'audio/jump.mp3',
'audio/ladder.mp3',
'audio/present.mp3',
'audio/tack.mp3'])
    ],

    author='Your Name',
    author_email='your.email@example.com',
    description='Willy the Worm game',
    license='MIT',
    keywords='game worm fun',
    url='http://example.com/'
)
