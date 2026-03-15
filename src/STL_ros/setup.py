#!/usr/bin/env python3

from distutils.core import setup
from catkin_pkg.python_setup import generate_distutils_setup

# fetch values from package.xml
setup_args = generate_distutils_setup(
    packages=['stl_ros'],
    package_dir={'': 'scripts'},
    requires=['rospy', 'std_msgs', 'geometry_msgs', 'nav_msgs']
)

setup(**setup_args)
