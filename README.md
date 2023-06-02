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
   vsgpoints mydata.asc
~~~~

To convert point cloud data to VulkanSceneGraph native format:

~~~ sh
   vsgpoints mydata.BIN -o mydata.vsgb
~~~~

Once you have converted to native .vsgb format you can load the data in any VulkanSceneGraph application:

~~~ sh
    vsgviewer mydata.vsgb
~~~

vsgpoints supports generating scene graphs in three ways, the command line options for these are:

| command line option | technique |
| --lod | using LOD's (the default) |
| --plod | Generation of paged databases |
| --flat |  flat group of point bricks |

~~~ sh
    # create vsg::LOD scene graph
    vsgpoints mydata.BIN --lad
    # or just rely upon the default being vsg::LOD scene graph
    vsgpoints mydata.BIN

    # create a flat group of point bricks
    vsgpoints mydata.BIN --flat

    # create a paged database, requires a specification of output file
    vsgpoints mydata.BIN -o paged.vsgb --plod
~~~

vsgpoints also supports generating points from meshed models, which can be enabled with --mesh

~~~ sh
    # load a mesh model and convert to points as input rather than required loading of points data.
    vsgpoints mymodel.gltf --mesh
~~~

If you don't include an output filename using ~ -o filename.vsb ~ them vsgpoints will automatically create a viewer to view the created scene graph, but if you output to a file no viewer will be created, but if you still want to viewer to appear then add -v option to force the viewer to be created.
