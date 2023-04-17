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
};

vsg::ref_ptr<vsg::Object> processRawData(const vsg::Path filename, const Settings& settings)
{
    double multiplier = 1.0 / settings.precision;
    double brickSize = settings.precision * pow(2.0, static_cast<double>(settings.bits));

    std::cout<<"sizeof(VsgIOPoint) = "<<sizeof(VsgIOPoint)<<std::endl;
    std::cout<<"brickSize = "<<brickSize<<std::endl;

    std::ifstream fin(filename, std::ios::in | std::ios::binary);
    if (!fin) return {};

    fin.seekg(0, fin.end);
    size_t fileSize = fin.tellg();
    size_t numPoints = fileSize / sizeof(VsgIOPoint);
    std::cout<<"    "<<filename<<" size = "<<format_number(fileSize)<<std::endl;
    std::cout<<"    numPoints = "<<format_number(numPoints)<<std::endl;

    if (numPoints == 0) return {};

    using Key = vsg::ivec4;

    std::map<Key, vsg::ref_ptr<Brick>> bricks;

    fin.clear();
    fin.seekg(0, fin.beg);

    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);
    size_t numPointsRead = 0;

    std::cout<<"Reading data."<<std::endl;
    vsg::dbox bounds;

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

            bounds.add(point.v);

            vsg::dvec3 scaled_v = point.v * multiplier;
            vsg::dvec3 rounded_v = {std::round(scaled_v.x), std::round(scaled_v.y), std::round(scaled_v.z)};
            vsg::t_vec3<int64_t> int64_v = {static_cast<int64_t>(rounded_v.x),  static_cast<int64_t>(rounded_v.y), static_cast<int64_t>(rounded_v.z)};
            Key key = { 256, static_cast<int32_t>(int64_v.x / 256), static_cast<int32_t>(int64_v.y / 256), static_cast<int32_t>(int64_v.z / 256)};

            PackedPoint packedPoint;
            packedPoint.v = { static_cast<uint8_t>(int64_v.x & 0xff), static_cast<uint8_t>(int64_v.y & 0xff), static_cast<uint8_t>(int64_v.z & 0xff)};
            packedPoint.c = point.c;

            auto& brick = bricks[key];
            if (!brick) brick = Brick::create();

            brick->points.push_back(packedPoint);
        }

    }

    std::cout<<"After reading data "<<bricks.size()<<std::endl;

    size_t biggestBrick = 0;
    vsg::t_box<int32_t> keyBounds;
    for(auto& [key, brick] : bricks)
    {
        keyBounds.add(key[1], key[2], key[3]);
        if (brick->points.size() > biggestBrick) biggestBrick = brick->points.size();
    }

    std::cout<<"bounds "<<bounds<<std::endl;
    std::cout<<"keyBounds "<<keyBounds<<std::endl;
    std::cout<<"biggest brick "<<biggestBrick<<std::endl;


#if 0
    for(auto& [key, brick] : bricks)
    {
        std::cout<<"key = "<<key<<" brick.size() = "<<brick->points.size()<<std::endl;
    }
#endif


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
