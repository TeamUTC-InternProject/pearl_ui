from setuptools import setup

setup(name='pearl_ui',
      version='0.1',
      description='User interface for PEARL threat detection system',
      url='https://github.com/TeamUTC-InternProject/pearl_ui',
      author='Chris Evers',
      author_email='chris.evers92@gmail.com',
      license='MIT',
      packages=['pearl_ui'],
      scripts=['bin/pearl'],
      zip_safe=False)

