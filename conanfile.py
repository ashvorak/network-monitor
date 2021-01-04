from conans import ConanFile, CMake

class ConanPackage(ConanFile):
    name = 'network-monitor'
    version = "0.1.0"

    generators = 'cmake_find_package'

    requires = [
        ('boost/1.74.0'),
    ]

    default_options = {
    		"boost:shared": True, 
    		"boost:without_fiber": True,
    		"boost:without_nowide": True
    }