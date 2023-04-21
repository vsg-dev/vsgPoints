#include <vsg/all.h>

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <iostream>
#include <sstream>
#include <iomanip>

#pragma pack(1)

struct VsgIOPoint
{
    vsg::dvec3 v;
    vsg::ubvec3 c;
};

struct PackedPoint
{
    vsg::ubvec3 v;
    vsg::ubvec3 c;
};

#pragma pack()


template<typename T>
std::string format_number(T v)
{
    std::vector<T> parts;
    do
    {
        parts.push_back(v % 1000);
        v /= 1000;
    } while (v > 0);


    std::stringstream s;
    auto itr = parts.rbegin();
    s << *(itr++);
    while(itr != parts.rend())
    {
        s << ',';
        s << std::setfill('0') << std::setw(3) << *(itr++);
    }

    return s.str();
}

class Brick : public vsg::Inherit<vsg::Object, Brick>
{
public:

    std::vector<PackedPoint> points;

    vsg::ref_ptr<vsg::Node> createRendering(const vsg::vec4& originScale)
    {
        auto positionScale = vsg::vec4Value::create(originScale);
        auto vertices = vsg::ubvec3Array::create(points.size());
        auto colors = vsg::ubvec3Array::create(points.size());

        auto vertex_itr = vertices->begin();
        auto color_itr = colors->begin();
        for(auto& point : points)
        {
            *(vertex_itr++) = point.v;
            *(color_itr++) = point.c;
        }

        // set up vertexDraw that will do the rendering.
        auto vertexDraw = vsg::VertexDraw::create();
        vertexDraw->assignArrays({positionScale, vertices, colors});
        vertexDraw->vertexCount = points.size();
        vertexDraw->instanceCount = 1;

        return vertexDraw;
    }
protected:
    virtual ~Brick() {}
};


struct Settings
{
    size_t numPointsPerBlock = 10000;
    double precision = 0.001;
    double bits = 8.0;
    bool allocate = false;
    bool write = false;
    vsg::Path extension = ".vsgt";
};

using Key = vsg::ivec4;
using Bricks = std::map<Key, vsg::ref_ptr<Brick>>;

bool readBricks(const vsg::Path filename, const Settings& settings, Bricks& bricks)
{
    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    if (!fin) return false;

    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();
    size_t numPoints = fileSize / sizeof(VsgIOPoint);
    std::cout<<"    "<<filename<<" size = "<<format_number(fileSize)<<std::endl;
    std::cout<<"    numPoints = "<<format_number(numPoints)<<std::endl;

    if (numPoints == 0) return false;

    fin.clear();
    fin.seekg(0, fin.beg);

    double multiplier = 1.0 / settings.precision;
    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);
    size_t numPointsRead = 0;

    std::cout<<"Reading data."<<std::endl;
    while(fin)
    {
        size_t numPointsToRead = std::min(numPoints - numPointsRead, settings.numPointsPerBlock);

        if (numPointsToRead == 0) break;

        fin.read(reinterpret_cast<char*>(points->dataPointer()), numPointsToRead * sizeof(VsgIOPoint));

        numPointsRead += numPointsToRead;

        for(auto& point : *points)
        {
            if (numPointsToRead==0) break;
            --numPointsToRead;

            vsg::dvec3 scaled_v = point.v * multiplier;
            vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
            vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
            Key key = { static_cast<int32_t>(int64_v.x / 256), static_cast<int32_t>(int64_v.y / 256), static_cast<int32_t>(int64_v.z / 256), 1};

            PackedPoint packedPoint;
            packedPoint.v = { static_cast<uint8_t>(int64_v.x & 0xff), static_cast<uint8_t>(int64_v.y & 0xff), static_cast<uint8_t>(int64_v.z & 0xff)};
            packedPoint.c = point.c;

            auto& brick = bricks[key];
            if (!brick)
            {
                brick = Brick::create();
            }

            brick->points.push_back(packedPoint);
        }

    }
    return true;
}

vsg::ref_ptr<vsg::Object> processRawData(const vsg::Path filename, const Settings& settings)
{
    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));

    std::cout<<"sizeof(VsgIOPoint) = "<<sizeof(VsgIOPoint)<<std::endl;
    std::cout<<"brickSize = "<<brickSize<<std::endl;

    Bricks bricks;
    if (!readBricks(filename, settings, bricks))
    {
        std::cout<<"Waring: unable to read file."<<std::endl;
        return {};
    }

    std::cout<<"After reading data "<<bricks.size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : bricks)
    {
        keyBounds.add(key.x, key.y, key.z);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }

    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;


#if 0
    for(auto& [key, brick] : bricks)
    {
        std::cout<<"key = "<<key<<" brick.size() = "<<brick->points.size()<<std::endl;
    }
#endif

    if (settings.write)
    {
        auto deliminator = vsg::Path::preferred_separator;
        vsg::Path path = vsg::filePath(filename);
        vsg::Path name = vsg::simpleFilename(filename);
        vsg::Path ext = settings.extension;

        std::basic_ostringstream<vsg::Path::value_type> str;

        for(auto& [key, brick] : bricks)
        {
            str.clear();
            str.str("");
            if (path) str << path << deliminator;
            str << name << deliminator << key.w << deliminator << key.z << deliminator << key.y;
            vsg::Path brick_path(str.str());

            str.clear();
            str.str("");
            str << key.x << ext;
            vsg::Path brick_filename(str.str());

            vsg::makeDirectory(brick_path);

            vsg::Path full_path = brick_path/brick_filename;

            vsg::vec4 originScale(static_cast<double>(key.x) * brickSize, static_cast<double>(key.y) * brickSize, static_cast<double>(key.z) * brickSize, brickSize);
            auto tile = brick->createRendering(originScale);
            vsg::write(tile, full_path);

            // std::cout<<"path = "<<brick_path<<"\tfilename = "<<brick_filename<<std::endl;
        }
    }


    vsg::ref_ptr<vsg::Objects> objects;
    return objects;
}


int main(int argc, char** argv)
{
    // set up defaults and read command line arguments to override them
    vsg::CommandLine arguments(&argc, argv);

    // set up defaults and read command line arguments to override them
    auto options = vsg::Options::create();
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();

#ifdef vsgXchange_all
    // add vsgXchange's support for reading and writing 3rd party file formats
    options->add(vsgXchange::all::create());
#endif

    if (int type; arguments.read("--allocator", type)) vsg::Allocator::instance()->allocatorType = vsg::AllocatorType(type);
    if (size_t objectsBlockSize; arguments.read("--objects", objectsBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_OBJECTS, objectsBlockSize);
    if (size_t nodesBlockSize; arguments.read("--nodes", nodesBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_NODES, nodesBlockSize);
    if (size_t dataBlockSize; arguments.read("--data", dataBlockSize)) vsg::Allocator::instance()->setBlockSize(vsg::ALLOCATOR_AFFINITY_DATA, dataBlockSize);


    std::cout<<"vsg::Allocator::instance()->allocatorType = "<<int(vsg::Allocator::instance()->allocatorType)<<std::endl;
    std::cout<<"vsg::Allocator::instance()->getMemoryBlocks(vsg::ALLOCATOR_AFFINITY_OBJECTS)->blockSize = "<<
        vsg::Allocator::instance()->getMemoryBlocks(vsg::ALLOCATOR_AFFINITY_OBJECTS)->blockSize<<std::endl;

    Settings settings;
    settings.numPointsPerBlock = arguments.value<size_t>(10000, "-b");
    settings.precision = arguments.value<double>(0.001, "-p");
    settings.bits = arguments.value<double>(8, "--bits");
    settings.allocate = arguments.read("-a");
    settings.write = arguments.read("-w");

    auto outputFilename = arguments.value<vsg::Path>("", "-o");

    auto objects = vsg::Objects::create();


    vsg::Path filename;
    while(arguments.read("-i", filename))
    {
        std::cout<<"filename = "<<filename<<std::endl;
        if (auto found_filename = vsg::findFile(filename, options))
        {
            if (auto object = processRawData(found_filename, settings))
            {
                objects->addChild(object);
            }
        }
    }

    if (outputFilename)
    {
        vsg::write(objects, outputFilename, options);
    }

    return 0;
}
