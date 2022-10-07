# vsgPoints - VulkanSceneGraph Point Cloud rendering

Cross platform, open source (MIT license) C++17 library and example set for rendering large point cloud data using VulkanSceneGraph.

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

# Example usage

Generate 1,000,000 point cloud and rendering with a single block of vertices, normals. colours, with vertex and normal data stored as float vec3:

   vsggeneratepoints -n 1000000
   vsggeneratepoints -n 1000000 --normals
   vsggeneratepoints -n 1000000 --colors
   vsggeneratepoints -n 1000000 --normals --colors

Generate 1,000,000 point cloud and rendering with bricks with vertex data stored in ubvec4 form and treated as fixed precision 0 to 1.0 x,y,z,w values

   vsggeneratepoints -n 1000000 --brick
   vsggeneratepoints -n 1000000 --normals --brick
   vsggeneratepoints -n 1000000 --colors --brick
   vsggeneratepoints -n 1000000 --normals --colors --brick

Stress test with 200,000,000 points:

   vsggeneratepoints -n 200000000 --colors --brick

Write created scene graph to disk as ascii file (.vsgt) :

   vsggeneratepoints -n 1000000 --colors --brick -o point_cloud.vsgt

Write created scene graph to disk as binary file (.vsgb) :

   vsggeneratepoints -n 1000000 --colors --brick -o point_cloud.vsgb
