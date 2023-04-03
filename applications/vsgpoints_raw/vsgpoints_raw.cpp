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

struct Brick
{
    size_t min = std::numeric_limits<size_t>::max();
    size_t max = std::numeric_limits<size_t>::lowest();
    size_t count = 0;
    vsg::dbox bound;
    vsg::ref_ptr<vsg::ubvec3Array> vertices;
    vsg::ref_ptr<vsg::ubvec3Array> colors;

    void add(size_t i, const vsg::dvec3& v)
    {
        if (i < min) min = i;
        if (i > max) max = i;
        ++count;
        bound.add(v);
    }

    void reset()
    {
        count = 0;
    }

    void set(const vsg::ubvec3& v, const vsg::ubvec3& c)
    {
        if (count >= vertices->size())
        {
            std::cout<<"Warning: count = "<<count<<" vertices->size() = "<<vertices->size()<<std::endl;
            //throw "problem";
            return;
        }
        if (count >= colors->size())
        {
            std::cout<<"Warning: count = "<<count<<" colors->size() = "<<colors->size()<<std::endl;
            return;
        }

        vertices->set(count, v);
        colors->set(count, c);
#if 0
        std::cout<<"set("<<count<<", v.x = "<<int(v.x)<<", c.y = "<<int(c.r)<<")"<<std::endl;
        std::cout<<"    result get("<<count<<", v.x = "<<int(vertices->at(count).x)<<", c.y = "<<int(c.r)<<")"<<std::endl;
#endif
        ++count;
    }


    void allocate()
    {
        vertices = vsg::ubvec3Array::create(count);
        colors = vsg::ubvec3Array::create(count);
    }
};

using Key = vsg::ivec3;
using Bricks = std::map<Key, Brick>;

struct Level
{
    Bricks bricks;
};

struct Settings
{
    size_t numPointsPerBlock = 10000;
    double precision = 0.001;
    double bits = 8.0;
    bool allocate = false;
};

vsg::ref_ptr<vsg::Object> processRawData(const vsg::Path filename, const Settings& settings)
{
    double multiplier = pow(2.0, static_cast<double>(settings.bits));
    double brickSize = settings.precision * multiplier;
    double inverse_brickSize = 1.0 / brickSize;

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

    vsg::dbox bounds;
    fin.seekg(0, fin.beg);
    auto points = vsg::Array<VsgIOPoint>::create(settings.numPointsPerBlock);
    size_t numPointsRead = 0;
    const double limit = std::numeric_limits<double>::max();
    vsg::dvec3 previous_point{limit, limit, limit};
    double min_distance = limit;
    size_t pointIndex = 0;


    VsgIOPoint firstPoint;
    fin.read(reinterpret_cast<char*>(&firstPoint), sizeof(VsgIOPoint));
    vsg::dvec3 origin = firstPoint.v;

    std::cout<<"    origin = "<<origin<<std::endl;
    Bricks bricks;

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

            if (previous_point != point.v)
            {
                auto distance = vsg::length(point.v - previous_point);
                if (distance < min_distance) min_distance = distance;
            }

            vsg::dvec3 cell_pos = (point.v - origin) * inverse_brickSize;
#if 0
            vsg::dvec3 cell_floor{ std::floor(cell_pos.x), std::floor(cell_pos.y), std::floor(cell_pos.z)};
            vsg::ivec3 cell_key{static_cast<int>(cell_floor.x), static_cast<int>(cell_floor.y), static_cast<int>(cell_floor.z)};
#else
            vsg::ivec3 cell_key{static_cast<int>(cell_pos.x), static_cast<int>(cell_pos.y), static_cast<int>(cell_pos.z)};
#endif
            Brick& brick = bricks[cell_key];
            brick.add(pointIndex, point.v);

            previous_point = point.v;
            ++pointIndex;
        }
    }

    vsg::dvec3 extents = bounds.max - bounds.min;

    double x_divisions = std::ceil(extents.x / brickSize);
    double y_divisions = std::ceil(extents.y / brickSize);
    double z_divisions = std::ceil(extents.z / brickSize);

    std::cout<<"  1st numPointsRead = "<<numPointsRead<<std::endl;
    std::cout<<"    bounds : min("<<bounds.min<<") max("<<bounds.max<<")"<<std::endl;
    std::cout<<"    width : "<<extents.x<<std::endl;
    std::cout<<"    depth : "<<extents.y<<std::endl;
    std::cout<<"    height : "<<extents.z<<std::endl;
    std::cout<<"    min_distance : "<<min_distance<<std::endl;

    std::cout<<"    x_divisions : "<<x_divisions<<std::endl;
    std::cout<<"    y_divisions : "<<y_divisions<<std::endl;
    std::cout<<"    z_divisions : "<<z_divisions<<std::endl;
    std::cout<<"    bricks "<<format_number(bricks.size())<<std::endl;

    size_t max_bricks = x_divisions * y_divisions * z_divisions;
    std::cout<<"      max_bricks "<<format_number(max_bricks)<<std::endl;
    std::cout<<"      bricks/max_bricks % "<<100.0 * double(bricks.size())/double(max_bricks)<<std::endl;

    size_t min_count = std::numeric_limits<size_t>::max();
    size_t max_count = std::numeric_limits<size_t>::lowest();
    size_t max_range = std::numeric_limits<size_t>::lowest();

    size_t total_memory = 0;
    size_t biggest_brick = std::numeric_limits<size_t>::lowest();

    for(auto& [key, brick] : bricks)
    {
        size_t range = brick.max - brick.min;
        if (brick.count < min_count) min_count = brick.count;
        if (brick.count > max_count) max_count = brick.count;
        if (range > max_range) max_range = range;

        size_t brick_memory = brick.count * ((settings.bits * 6) / 8);
        total_memory += brick_memory;

        if (brick_memory > biggest_brick) biggest_brick = brick_memory;
    }

    if (settings.allocate)
    {
        for(auto& [key, brick] : bricks)
        {
            brick.allocate();
            brick.reset();
        }

        numPointsRead = 0;
        pointIndex = 0;
        fin.clear();
        fin.seekg(0, fin.beg);

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

                vsg::dvec3 cell_pos = (point.v - origin) * inverse_brickSize;
#if 0
                vsg::dvec3 cell_floor{ std::floor(cell_pos.x), std::floor(cell_pos.y), std::floor(cell_pos.z)};
                vsg::ivec3 cell_key{static_cast<int>(cell_floor.x), static_cast<int>(cell_floor.y), static_cast<int>(cell_floor.z)};
                vsg::dvec3 cell_r = (cell_pos - cell_floor) * multiplier;
#else
                vsg::ivec3 cell_key{static_cast<int>(cell_pos.x), static_cast<int>(cell_pos.y), static_cast<int>(cell_pos.z)};
                vsg::dvec3 cell_r = (cell_pos - vsg::dvec3{static_cast<double>(cell_key.x), static_cast<double>(cell_key.y), static_cast<double>(cell_key.z)}) * multiplier;
#endif
                vsg::ubvec3 v{static_cast<std::uint8_t>(cell_r.x), static_cast<std::uint8_t>(cell_r.y), static_cast<std::uint8_t>(cell_r.z)};
                Brick& brick = bricks[cell_key];
                brick.set(v, point.c);

                ++pointIndex;
            }
        }
    }

    std::cout<<"    min_count "<< format_number(min_count)<<std::endl;
    std::cout<<"    max_count "<< format_number(max_count)<<std::endl;
    std::cout<<"    max_range "<< format_number(max_range)<<std::endl;
    std::cout<<"    biggest_brick "<< format_number(biggest_brick)<<" bytes"<<std::endl;
    std::cout<<"    total_memory "<< format_number(total_memory)<<" bytes"<<std::endl;

    std::cout<<"  2nd numPointsRead = "<<numPointsRead<<std::endl;

    vsg::ivec3 min_key = { std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::max() };
    vsg::ivec3 max_key = { std::numeric_limits<int>::lowest(), std::numeric_limits<int>::lowest(), std::numeric_limits<int>::lowest() };

    for(auto& [key, brick] : bricks)
    {
        if (key.x < min_key.x) min_key.x = key.x;
        if (key.y < min_key.y) min_key.y = key.y;
        if (key.z < min_key.z) min_key.z = key.z;
        if (key.x > max_key.x) max_key.x = key.x;
        if (key.y > max_key.y) max_key.y = key.y;
        if (key.z > max_key.z) max_key.z = key.z;
    }

    vsg::ivec3 range_key = max_key - min_key;
    std::cout<<"    min_key "<<min_key<<std::endl;
    std::cout<<"    max_key "<<max_key<<std::endl;
    std::cout<<"    range_key "<<range_key<<std::endl;

    std::vector<vsg::ivec3> levelRanges;

    while(range_key.x > 1 || range_key.y > 1 || range_key.z > 1)
    {
        levelRanges.push_back(range_key);
        if (range_key.x >= 2) { range_key.x = (range_key.x+1)/2; }
        if (range_key.y >= 2) { range_key.y = (range_key.y+1)/2; }
        if (range_key.z >= 2) { range_key.z = (range_key.z+1)/2; }
    }
    levelRanges.push_back(range_key);

    size_t num_levels = levelRanges.size();
    std::cout<<"    num_levels = "<<num_levels<<std::endl;
    size_t level = 0;
    for(auto itr = levelRanges.rbegin(); itr != levelRanges.rend(); ++itr)
    {
        std::cout<<"    level = "<<level<<", range "<<*itr<<std::endl;
        ++level;
    }
    std::cout<<std::endl;

    auto objects = vsg::Objects::create();
    for(auto& [key, brick] : bricks)
    {
        if (brick.vertices) objects->addChild(brick.vertices);
        if (brick.colors) objects->addChild(brick.colors);
    }
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
