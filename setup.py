from distutils.core import setup

setup(
    name = 'willytheworm',
    packages = ['willytheworm'],
    version = '1.0-1138',  # Ideally should be same as your github release tag varsion
    description = 'Willy the Worm clone',
    author = 'Jason Hall',
    author_email = 'jasonbrianhall@gmail.com',
    url = 'https://github.com/jasonbrianhall/willytheworm',
    download_url = 'https://github.com/jasonbrianhall/willytheworm',
    keywords = ['video game', 'pygame', 'arcade'],
    classifiers = [],
    package_data={'': ['willy.chr', 'utilities/*', 'willytheworm', 'edwillytheworm', 'audio/*.mp3', 'levels.json']}
)
