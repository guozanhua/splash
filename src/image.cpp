#include "image.h"

#include <fstream>
#include <memory>
#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebufalgo.h>

#include "log.h"
#include "osUtils.h"
#include "threadpool.h"
#include "timer.h"

#define SPLASH_IMAGE_COPY_THREADS 4

using namespace std;

namespace Splash {

/*************/
Image::Image()
{
    init();
}

/*************/
Image::Image(bool linked)
{
    init();
    _linkedToWorldObject = linked;
}

/*************/
Image::Image(oiio::ImageSpec spec)
{
    init();
    set(spec.width, spec.height, spec.nchannels, spec.format);
}

/*************/
void Image::init()
{
    _type = "image";
    oiio::attribute("threads", 0); // Disable the thread limitation for OIIO

    createDefaultImage();
    registerAttributes();
}

/*************/
Image::~Image()
{
    unique_lock<mutex> writeLock(_writeMutex);
    unique_lock<mutex> readlock(_readMutex);
#ifdef DEBUG
    Log::get() << Log::DEBUGGING << "Image::~Image - Destructor" << Log::endl;
#endif
}

/*************/
const void* Image::data() const
{
    if (_image)
        return _image->localpixels();
    else
        return nullptr;
}

/*************/
oiio::ImageBuf Image::get() const
{
    oiio::ImageBuf img;
    unique_lock<mutex> lock(_readMutex);
    if (_image)
        img.copy(*_image);
    return img;
}

/*************/
oiio::ImageSpec Image::getSpec() const
{
    unique_lock<mutex> lock(_readMutex);
    if (_image)
        return _image->spec();
    else
        return oiio::ImageSpec();
}

/*************/
void Image::set(const oiio::ImageBuf& img)
{
    unique_lock<mutex> lockRead(_readMutex);
    _image->copy(img);
}

/*************/
void Image::set(unsigned int w, unsigned int h, unsigned int channels, oiio::TypeDesc type)
{
    oiio::ImageSpec spec(w, h, channels, type);
    oiio::ImageBuf img(spec);

    unique_lock<mutex> lock(_readMutex);
    if (!_image)
        _image = unique_ptr<oiio::ImageBuf>(new oiio::ImageBuf());
    _image->swap(img);
    updateTimestamp();
}

/*************/
shared_ptr<SerializedObject> Image::serialize() const
{
    unique_lock<mutex> lock(_readMutex);

    if (Timer::get().isDebug())
        Timer::get() << "serialize " + _name;

    // We first get the xml version of the specs, and pack them into the obj
    if (!_image)
        return {};
    string xmlSpec = _image->spec().to_xml();
    int nbrChar = xmlSpec.size();
    int imgSize = _image->spec().pixel_bytes() * _image->spec().width * _image->spec().height;
    int totalSize = sizeof(nbrChar) + nbrChar + imgSize;
    
    auto obj = make_shared<SerializedObject>(totalSize);

    auto currentObjPtr = obj->data();
    const char* ptr = reinterpret_cast<const char*>(&nbrChar);
    copy(ptr, ptr + sizeof(nbrChar), currentObjPtr);
    currentObjPtr += sizeof(nbrChar);

    const char* charPtr = reinterpret_cast<const char*>(xmlSpec.c_str());
    copy(charPtr, charPtr + nbrChar, currentObjPtr);
    currentObjPtr += nbrChar;

    // And then, the image
    const char* imgPtr = reinterpret_cast<const char*>(_image->localpixels());
    if (imgPtr == NULL)
        return {};
    
    vector<unsigned int> threadIds;
    int stride = SPLASH_IMAGE_COPY_THREADS;
    for (int i = 0; i < stride - 1; ++i)
    {
        threadIds.push_back(SThread::pool.enqueue([=]() {
            copy(imgPtr + imgSize / stride * i, imgPtr + imgSize / stride * (i + 1), currentObjPtr + imgSize / stride * i);
        }));
    }
    copy(imgPtr + imgSize / stride * (stride - 1), imgPtr + imgSize, currentObjPtr + imgSize / stride * (stride - 1));
    SThread::pool.waitThreads(threadIds);

    if (Timer::get().isDebug())
        Timer::get() >> "serialize " + _name;

    return obj;
}

/*************/
bool Image::deserialize(shared_ptr<SerializedObject> obj)
{
    if (obj.get() == nullptr || obj->size() == 0)
        return false;

    if (Timer::get().isDebug())
        Timer::get() << "deserialize " + _name;

    // First, we get the size of the metadata
    int nbrChar;
    char* ptr = reinterpret_cast<char*>(&nbrChar);

    auto currentObjPtr = obj->data();
    copy(currentObjPtr, currentObjPtr + sizeof(nbrChar), ptr);
    currentObjPtr += sizeof(nbrChar);

    try
    {
        unique_lock<mutex> lockWrite(_writeMutex);

        char xmlSpecChar[nbrChar];
        ptr = reinterpret_cast<char*>(xmlSpecChar);
        copy(currentObjPtr, currentObjPtr + nbrChar, ptr);
        currentObjPtr += nbrChar;
        string xmlSpec(xmlSpecChar);

        oiio::ImageSpec spec;
        spec.from_xml(xmlSpec.c_str());

        oiio::ImageSpec curSpec = _bufferDeserialize.spec();
        if (spec.width != curSpec.width || spec.height != curSpec.height || spec.nchannels != curSpec.nchannels || spec.format != curSpec.format)
            _bufferDeserialize.reset(spec);

        int imgSize = _bufferDeserialize.spec().pixel_bytes() * _bufferDeserialize.spec().width * _bufferDeserialize.spec().height;
        ptr = reinterpret_cast<char*>(_bufferDeserialize.localpixels());

        vector<unsigned int> threadIds;
        int stride = SPLASH_IMAGE_COPY_THREADS;
        for (int i = 0; i < stride - 1; ++i)
        {
            threadIds.push_back(SThread::pool.enqueue([=]() {
                copy(currentObjPtr + imgSize / stride * i, currentObjPtr + imgSize / stride * (i + 1), ptr + imgSize / stride * i);
            }));
        }
        copy(currentObjPtr + imgSize / stride * (stride - 1), currentObjPtr + imgSize, ptr + imgSize / stride * (stride - 1));
        SThread::pool.waitThreads(threadIds);

        if (!_bufferImage)
            _bufferImage = unique_ptr<oiio::ImageBuf>(new oiio::ImageBuf());
        _bufferImage->swap(_bufferDeserialize);
        _imageUpdated = true;

        updateTimestamp();
    }
    catch (...)
    {
        Log::get() << Log::ERROR << "Image::" << __FUNCTION__ << " - Unable to deserialize the given object" << Log::endl;
        return false;
    }

    if (Timer::get().isDebug())
        Timer::get() >> "deserialize " + _name;

    return true;
}

/*************/
bool Image::read(const string& filename)
{
    _filepath = filename;
    if (!_linkedToWorldObject)
        return readFile(filename);
    else
        return true;
}

/*************/
bool Image::readFile(const string& filename)
{
    auto filepath = string(filename);
    if (Utils::getPathFromFilePath(filepath) == "" || filepath.find(".") == 0)
        filepath = _configFilePath + filepath;

    _filepath = filepath;

    if (!ifstream(_filepath).is_open())
    {
        Log::get() << Log::WARNING << "Image::" << __FUNCTION__ << " - Unable to load file " << filename << Log::endl;
        return false;
    }

    try
    {
        auto in = oiio::ImageInput::open(filepath);

        if (!in)
        {
            Log::get() << Log::WARNING << "Image::" << __FUNCTION__ << " - Unable to load file " << filename << Log::endl;
            return false;
        }

        const oiio::ImageSpec& spec = in->spec();
        if (spec.format != oiio::TypeDesc::UINT8)
        {
            Log::get() << Log::WARNING << "Image::" << __FUNCTION__ << " - Only 8bit images are supported." << Log::endl;
            return false;
        }

        int xres = spec.width;
        int yres = spec.height;
        int channels = spec.nchannels;
        oiio::ImageBuf img(spec); 
        in->read_image(oiio::TypeDesc::UINT8, img.localpixels());

        in->close();
        delete in;

        if (channels != 3 && channels != 4)
            return false;

        unique_lock<mutex> lock(_writeMutex);
        if (!_bufferImage)
            _bufferImage = unique_ptr<oiio::ImageBuf>(new oiio::ImageBuf());
        _bufferImage->swap(img);
        _imageUpdated = true;

        updateTimestamp();

        return true;
    }
    catch (const exception& e)
    {
        Log::get() << Log::WARNING << "Image::" << __FUNCTION__ << " - Caught an exception while opening image file: " << e.what() << Log::endl;
        return false;
    }
}

/*************/
void Image::setTo(float value)
{
    unique_lock<mutex> lock(_readMutex);
    if (!_image)
        return;

    float v[_image->nchannels()];
    for (int i = 0; i < _image->nchannels(); ++i)
        v[i] = (float)value;
    oiio::ImageBufAlgo::fill(*_image, v);
}

/*************/
void Image::update()
{
    unique_lock<mutex> lockRead(_readMutex);
    unique_lock<mutex> lockWrite(_writeMutex);
    if (_imageUpdated)
    {
        _image.swap(_bufferImage);
        _imageUpdated = false;
    }
    else if (_benchmark)
        updateTimestamp();
}

/*************/
bool Image::write(const std::string& filename)
{
    oiio::ImageOutput* out = oiio::ImageOutput::create(filename);
    if (!out)
        return false;

    unique_lock<mutex> lock(_readMutex);
    if (!_image)
        return false;
    out->open(filename, _image->spec());
    out->write_image(_image->spec().format, _image->localpixels());
    out->close();
    delete out;

    return true;
}

/*************/
void Image::createDefaultImage()
{
    oiio::ImageSpec spec(512, 512, 4, oiio::TypeDesc::UINT8);
    oiio::ImageBuf img(spec);

    for (oiio::ImageBuf::Iterator<unsigned char> p(img); !p.done(); ++p)
    {
        if (!p.exists())
            continue;

        if (p.x() % 16 > 7 && p.y() % 64 > 31)
            for (int c = 0; c < img.nchannels(); ++c)
                p[c] = 255;
        else
            for (int c = 0; c < img.nchannels(); ++c)
                p[c] = 0;
    }

    unique_lock<mutex> lock(_readMutex);
    if (!_image)
        _image = unique_ptr<oiio::ImageBuf>(new oiio::ImageBuf());
    _image->swap(img);
    updateTimestamp();
}

/*************/
void Image::registerAttributes()
{
    _attribFunctions["flip"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        _flip = (args[0].asInt() > 0) ? true : false;
        return true;
    }, [&]() -> Values {
        return {_flip};
    });

    _attribFunctions["flop"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        _flop = (args[0].asInt() > 0) ? true : false;
        return true;
    }, [&]() -> Values {
        return {_flop};
    });

    _attribFunctions["file"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        return read(args[0].asString());
    }, [&]() -> Values {
        return {_filepath};
    });

    _attribFunctions["srgb"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        _srgb = (args[0].asInt() > 0) ? true : false;     
        return true;
    }, [&]() -> Values {
        return {_srgb};
    });

    _attribFunctions["benchmark"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        if (args[0].asInt() > 0)
            _benchmark = true;
        else
            _benchmark = false;
        return true;
    });
}

} // end of namespace
