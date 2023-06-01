# vsgPoints - VulkanSceneGraph Point Cloud rendering

Cross platform, open source (MIT license) C++17 library and example set for rendering large (hundreds of millions) point cloud data using VulkanSceneGraph.

The vsgPoints project contains a vsgPoints library that applications may link to add point cloud loading and scene graph creation capabilities to their applications, and vsgpoints utility program.

## Dependencies

3rd party dependencies:

    C++17
    CMake
    VukanSDK
    GLslang
    VulkanSceneGraph
    vsgXchange

## Building vsgPoints

Unix in source build:

    cd vsgPoints
    cmake .
    make -j 8
    make install

# vsgpoints application usage

To view .3dc or .asc point clouds, or .BIN (double x,y,z; uint8_t r, g, b) data :

~~~ sh
   vsgpoints -i mydata.asc
~~~~

To convert point cloud data to VulkanSceneGraph native format:

~~~ sh
   vsgpoints -i mydata.BIN -o mydata.vsgb
~~~~

Once you have converted to native .vsgb format you can load the data in any VulkanSceneGraph application:

~~~ sh
    vsgviewer mydata.vsgb
~~~

