#include "window.h"

#include "camera.h"
#include "geometry.h"
#include "gui.h"
#include "image.h"
#include "log.h"
#include "object.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "texture_image.h"
#include "timer.h"

#include <functional>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace std::placeholders;

namespace Splash {

/*************/
mutex Window::_callbackMutex;
deque<pair<GLFWwindow*, vector<int>>> Window::_keys;
deque<pair<GLFWwindow*, unsigned int>> Window::_chars;
deque<pair<GLFWwindow*, vector<int>>> Window::_mouseBtn;
pair<GLFWwindow*, vector<double>> Window::_mousePos;
deque<pair<GLFWwindow*, vector<double>>> Window::_scroll;
vector<string> Window::_pathDropped;
atomic_bool Window::_quitFlag;

atomic_int Window::_swappableWindowsCount {0};

/*************/
Window::Window(RootObjectWeakPtr root)
       : BaseObject(root)
{
    _type = "window";

    ScenePtr scene = dynamic_pointer_cast<Scene>(root.lock());
    GlWindowPtr w = scene->getNewSharedWindow();
    if (w.get() == nullptr)
        return;

    _window = w;
    _isInitialized = setProjectionSurface();
    if (!_isInitialized)
        Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - Error while creating the Window" << Log::endl;
    else
        Log::get() << Log::MESSAGE << "Window::" << __FUNCTION__ << " - Window created successfully" << Log::endl;

    _viewProjectionMatrix = glm::ortho(-1.f, 1.f, -1.f, 1.f);

    setEventsCallbacks();
    registerAttributes();
    showCursor(false);

    // Get the default window size and position
    glfwGetWindowPos(_window->get(), &_windowRect[0], &_windowRect[1]);
    glfwGetFramebufferSize(_window->get(), &_windowRect[2], &_windowRect[3]);

    // Create the render FBO
    glGetError();
    glGenFramebuffers(1, &_renderFbo);
    setupRenderFBO();

    glBindFramebuffer(GL_FRAMEBUFFER, _renderFbo);
    GLenum _status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (_status != GL_FRAMEBUFFER_COMPLETE)
        Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - Error while initializing render framebuffer object: " << _status << Log::endl;
    else
        Log::get() << Log::MESSAGE << "Window::" << __FUNCTION__ << " - Render framebuffer object successfully initialized" << Log::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // And the read framebuffer
    setupReadFBO();
}

/*************/
Window::~Window()
{
#ifdef DEBUG
    Log::get() << Log::DEBUGGING << "Window::~Window - Destructor" << Log::endl;
#endif

    glDeleteFramebuffers(1, &_renderFbo);
    glDeleteFramebuffers(1, &_readFbo);
}

/*************/
int Window::getChars(GLFWwindow*& win, unsigned int& codepoint)
{
    unique_lock<mutex> lock(_callbackMutex);
    if (_chars.size() == 0)
        return 0;

    win = _chars.front().first;
    codepoint = _chars.front().second;

    _chars.pop_front();

    return _chars.size() + 1;
}

/*************/
bool Window::getKey(int key)
{
    if (glfwGetKey(_window->get(), key) == GLFW_PRESS)
        return true;
    return false;
}

/*************/
int Window::getKeys(GLFWwindow*& win, int& key, int& action, int& mods)
{
    unique_lock<mutex> lock(_callbackMutex);
    if (_keys.size() == 0)
        return 0;

    win = _keys.front().first;
    vector<int> keys = _keys.front().second;

    key = keys[0];
    action = keys[2];
    mods = keys[3];

    _keys.pop_front();

    return _keys.size() + 1;
}

/*************/
int Window::getMouseBtn(GLFWwindow*& win, int& btn, int& action, int& mods)
{
    unique_lock<mutex> lock(_callbackMutex);
    if (_mouseBtn.size() == 0)
        return 0;

    win = _mouseBtn.front().first;
    vector<int> mouse = _mouseBtn.front().second;

    btn = mouse[0];
    action = mouse[1];
    mods = mouse[2];

    _mouseBtn.pop_front();

    return _mouseBtn.size() + 1;
}

/*************/
void Window::getMousePos(GLFWwindow*& win, int& xpos, int& ypos)
{
    unique_lock<mutex> lock(_callbackMutex);
    if (_mousePos.second.size() != 2)
        return;

    win = _mousePos.first;
    xpos = (int)_mousePos.second[0];
    ypos = (int)_mousePos.second[1];
}

/*************/
int Window::getScroll(GLFWwindow*& win, double& xoffset, double& yoffset)
{
    unique_lock<mutex> lock(_callbackMutex);
    if (_scroll.size() == 0)
        return 0;

    win = _scroll.front().first;
    xoffset = _scroll.front().second[0];
    yoffset = _scroll.front().second[1];

    _scroll.pop_front();

    return _scroll.size() + 1;
}

/*************/
vector<string> Window::getPathDropped()
{
    unique_lock<mutex> lock(_callbackMutex);
    auto paths = _pathDropped;
    _pathDropped.clear();
    return paths;
}

/*************/
bool Window::linkTo(shared_ptr<BaseObject> obj)
{
    // Mandatory before trying to link
    if (!BaseObject::linkTo(obj))
        return false;

    if (dynamic_pointer_cast<Texture>(obj).get() != nullptr)
    {
        TexturePtr tex = dynamic_pointer_cast<Texture>(obj);
        setTexture(tex);
        return true;
    }
    else if (dynamic_pointer_cast<Image>(obj).get() != nullptr)
    {
        auto tex = make_shared<Texture_Image>();
        tex->setName(getName() + "_" + obj->getName() + "_tex");
        tex->setAttribute("resizable", {0});
        if (tex->linkTo(obj))
        {
            _root.lock()->registerObject(tex);
            return linkTo(tex);
        }
        else
            return false;
    }
    else if (dynamic_pointer_cast<Camera>(obj).get() != nullptr)
    {
        CameraPtr cam = dynamic_pointer_cast<Camera>(obj);
        for (auto& tex : cam->getTextures())
            setTexture(tex);
        return true;
    }
    else if (dynamic_pointer_cast<Gui>(obj).get() != nullptr)
    {
        if (_guiTexture != nullptr)
            _screenGui->removeTexture(_guiTexture);
        GuiPtr gui = dynamic_pointer_cast<Gui>(obj);
        _guiTexture = gui->getTexture();
        _screenGui->addTexture(_guiTexture);
        return true;
    }

    return false;
}

/*************/
bool Window::unlinkFrom(shared_ptr<BaseObject> obj)
{
    if (dynamic_pointer_cast<Texture>(obj).get() != nullptr)
    {
        TexturePtr tex = dynamic_pointer_cast<Texture>(obj);
        unsetTexture(tex);
    }
    else if (dynamic_pointer_cast<Image>(obj).get() != nullptr)
    {
        // Look for the corresponding texture
        string texName = getName() + "_" + obj->getName() + "_tex";
        TexturePtr tex = nullptr;
        for (auto& inTex : _inTextures)
        {
            if (inTex.expired())
                continue;
            auto lockedTex = inTex.lock();
            if (lockedTex->getName() == texName)
                tex = lockedTex;
        }
        if (tex != nullptr)
        {
            tex->unlinkFrom(obj);
            unsetTexture(tex);
        }
    }
    else if (dynamic_pointer_cast<Camera>(obj).get() != nullptr)
    {
        CameraPtr cam = dynamic_pointer_cast<Camera>(obj);
        for (auto& tex : cam->getTextures())
            unsetTexture(tex);
    }
    else if (dynamic_pointer_cast<Gui>(obj).get() != nullptr)
    {
        GuiPtr gui = dynamic_pointer_cast<Gui>(obj);
        if (gui->getTexture() == _guiTexture)
        {
            _screenGui->removeTexture(_guiTexture);
            _guiTexture = nullptr;
        }
    }

    return BaseObject::unlinkFrom(obj);
}

/*************/
bool Window::render()
{
    // Update the FBO configuration if needed
    setupRenderFBO();

    int w, h;
    glfwGetFramebufferSize(_window->get(), &w, &h);
    glViewport(0, 0, w, h);

#ifdef DEBUG
    glGetError();
#endif

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _renderFbo);
    GLenum fboBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, fboBuffers);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (_srgb)
        glEnable(GL_FRAMEBUFFER_SRGB);

    // If we are in synchronization testing mode
    if (_swapSynchronizationTesting)
    {
        glClearColor(_swapSynchronizationColor[0], _swapSynchronizationColor[1], _swapSynchronizationColor[2], _swapSynchronizationColor[3]); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    // else, we draw the window normally
    else
    {
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);

        _screen->getShader()->setAttribute("layout", _layout);
        _screen->getShader()->setAttribute("uniform", {"_gamma", (float)_srgb, _gammaCorrection}); 
        _screen->activate();
        _screen->draw();
        _screen->deactivate();
    }

    if (_guiTexture != nullptr)
    {
        _screenGui->activate();
        _screenGui->draw();
        _screenGui->deactivate();
    }

    glDeleteSync(_renderFence);
    _renderFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    _swappableWindowsCount = 0; // Reset the window number

    // Resize the input textures accordingly to the window size.
    // This goes upstream to the cameras and gui
    // Textures are resized to the number of "frame" there are, according to the layout
    bool resize = true;
    for (int i = 0; i < _inTextures.size(); ++i)
    {
        int value = _layout[i].asInt();
        for (int j = i + 1; j < _inTextures.size(); ++j)
            if (_layout[j].asInt() != value)
                resize = false;
    }
    if (resize) // We don't do this if we are directly connected to a Texture (updated from an image)
    {
        for (auto& t : _inTextures)
        {
            if (t.expired())
                continue;
            t.lock()->setAttribute("size", {w, h});
        }   
    }
    if (_guiTexture != nullptr)
        _guiTexture->setAttribute("size", {w, h});

#ifdef DEBUG
    GLenum error = glGetError();
    if (error)
        Log::get() << Log::WARNING << _type << "::" << __FUNCTION__ << " - Error while rendering the window: " << error << Log::endl;
#endif

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

#ifdef DEBUG
    return error != 0 ? true : false;
#else
    return false;
#endif
}

/*************/
void Window::setupRenderFBO()
{
    glfwGetWindowPos(_window->get(), &_windowRect[0], &_windowRect[1]);
    glfwGetFramebufferSize(_window->get(), &_windowRect[2], &_windowRect[3]);

    glBindFramebuffer(GL_FRAMEBUFFER, _renderFbo);

    if (!_depthTexture)
    {
        _depthTexture = make_shared<Texture_Image>(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 512, 512, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture->getTexId(), 0);
    }
    else
    {
        _depthTexture->setAttribute("resizable", {1});
        _depthTexture->setAttribute("size", {_windowRect[2], _windowRect[3]});
        _depthTexture->setAttribute("resizable", {0});
    }

    if (!_colorTexture)
    {
        _colorTexture = make_shared<Texture_Image>();
        _colorTexture->setAttribute("filtering", {0});
        _colorTexture->reset(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, _windowRect[2], _windowRect[3], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTexture->getTexId(), 0);
    }
    else
    {
        _colorTexture->setAttribute("resizable", {1});
        _colorTexture->setAttribute("size", {_windowRect[2], _windowRect[3]});
        _colorTexture->setAttribute("resizable", {0});
    }

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/*************/
void Window::setupReadFBO()
{
    _window->setAsCurrentContext();

    if (_readFbo != 0)
        glDeleteFramebuffers(1, &_readFbo);

    glGenFramebuffers(1, &_readFbo);

    glBindFramebuffer(GL_FRAMEBUFFER, _readFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _colorTexture->getTexId(), 0);
    GLenum _status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (_status != GL_FRAMEBUFFER_COMPLETE)
        Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - Error while initializing read framebuffer object: " << _status << Log::endl;
    else
        Log::get() << Log::MESSAGE << "Window::" << __FUNCTION__ << " - Read framebuffer object successfully initialized" << Log::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    _window->releaseContext();
}

/*************/
void Window::swapBuffers()
{
    if (!_window->setAsCurrentContext()) 
    	 Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;

    glFlush();
    glWaitSync(_renderFence, 0, GL_TIMEOUT_IGNORED);

    // Only one window will wait for vblank, the others draws directly into front buffer
    auto windowIndex = _swappableWindowsCount.fetch_add(1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, _readFbo);

    // If swap interval is null (meaning no vsync), draw directly to the front buffer in any case
#if HAVE_OSX
    glDrawBuffer(GL_BACK);
#else
    if (windowIndex != 0)
        glDrawBuffer(GL_FRONT);
    else
        glDrawBuffer(GL_BACK);
#endif

    glBlitFramebuffer(0, 0, _windowRect[2], _windowRect[3],
                      0, 0, _windowRect[2], _windowRect[3],
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#if HAVE_OSX
    glfwSwapBuffers(_window->get());
#else
    if (windowIndex == 0)
        glfwSwapBuffers(_window->get());
#endif

    _window->releaseContext();
}

/*************/
void Window::showCursor(bool visibility)
{
    if (visibility)
        glfwSetInputMode(_window->get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    else
        glfwSetInputMode(_window->get(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
}

/*************/
bool Window::switchFullscreen(int screenId)
{
    int count;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (screenId >= count)
        return false;

    if (_window.get() == nullptr)
        return false;

    if (screenId != -1)
        _screenId = screenId;
    else if (screenId == _screenId)
        return true;

    const GLFWvidmode* vidmode = glfwGetVideoMode(monitors[_screenId]);

    glfwWindowHint(GLFW_VISIBLE, true);
    GLFWwindow* window;
    if (glfwGetWindowMonitor(_window->get()) == NULL)
    {
        glfwWindowHint(GLFW_RED_BITS, vidmode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, vidmode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, vidmode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, vidmode->refreshRate);

        window = glfwCreateWindow(vidmode->width, vidmode->height, ("Splash::" + _name).c_str(), monitors[_screenId], _window->getMainWindow());
    }
    else
        window = glfwCreateWindow(vidmode->width, vidmode->height, ("Splash::" + _name).c_str(), 0, _window->getMainWindow());

    if (!window)
    {
        Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - Unable to create new fullscreen shared window" << Log::endl;
        return false;
    }

    _window = move(make_shared<GlWindow>(window, _window->getMainWindow()));
    updateSwapInterval();
    setupReadFBO();

    setEventsCallbacks();
    showCursor(false);

    return true;
}

/*************/
void Window::setTexture(TexturePtr tex)
{
    auto textureIt = find_if(_inTextures.begin(), _inTextures.end(), [&](const weak_ptr<Texture>& t) {
        if (t.expired())
            return false;
        auto texture = t.lock();
        if (texture == tex)
            return true;
        return false;
    });

    if (textureIt != _inTextures.end())
        return;

    _inTextures.push_back(tex);
    _screen->addTexture(tex);
}

/*************/
void Window::unsetTexture(TexturePtr tex)
{
    auto textureIt = find_if(_inTextures.begin(), _inTextures.end(), [&](const weak_ptr<Texture>& t) {
        if (t.expired())
            return false;
        auto texture = t.lock();
        if (texture == tex)
            return true;
        return false;
    });

    if (textureIt != _inTextures.end())
    {
        _inTextures.erase(textureIt);
        _screen->removeTexture(tex);
    }
}

/*************/
void Window::keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    unique_lock<mutex> lock(_callbackMutex);
    vector<int> keys {key, scancode, action, mods};
    _keys.push_back(pair<GLFWwindow*, vector<int>>(win, keys));
}

/*************/
void Window::charCallback(GLFWwindow* win, unsigned int codepoint)
{
    unique_lock<mutex> lock(_callbackMutex);
    _chars.push_back(pair<GLFWwindow*, unsigned int>(win, codepoint));
}

/*************/
void Window::mouseBtnCallback(GLFWwindow* win, int button, int action, int mods)
{
    unique_lock<mutex> lock(_callbackMutex);
    vector<int> btn {button, action, mods};
    _mouseBtn.push_back(pair<GLFWwindow*, vector<int>>(win,btn));
}

/*************/
void Window::mousePosCallback(GLFWwindow* win, double xpos, double ypos)
{
    unique_lock<mutex> lock(_callbackMutex);
    vector<double> pos {xpos, ypos};
    _mousePos.first = win;
    _mousePos.second = move(pos);
}

/*************/
void Window::scrollCallback(GLFWwindow* win, double xoffset, double yoffset)
{
    unique_lock<mutex> lock(_callbackMutex);
    vector<double> scroll {xoffset, yoffset};
    _scroll.push_back(pair<GLFWwindow*, vector<double>>(win, scroll));
}

/*************/
void Window::pathdropCallback(GLFWwindow* win, int count, const char** paths)
{
    unique_lock<mutex> lock(_callbackMutex);
    for (int i = 0; i < count; ++i)
        _pathDropped.push_back(string(paths[i]));
}

/*************/
void Window::closeCallback(GLFWwindow* win)
{
    unique_lock<mutex> lock(_callbackMutex);
    _quitFlag = true;
}

/*************/
void Window::setEventsCallbacks()
{
    glfwSetKeyCallback(_window->get(), Window::keyCallback);
    glfwSetCharCallback(_window->get(), Window::charCallback);
    glfwSetMouseButtonCallback(_window->get(), Window::mouseBtnCallback);
    glfwSetCursorPosCallback(_window->get(), Window::mousePosCallback);
    glfwSetScrollCallback(_window->get(), Window::scrollCallback);
    glfwSetDropCallback(_window->get(), Window::pathdropCallback);
    glfwSetWindowCloseCallback(_window->get(), Window::closeCallback);
}

/*************/
bool Window::setProjectionSurface()
{
    if (!_window->setAsCurrentContext()) 
    	 Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;
    glfwShowWindow(_window->get());
    glfwSwapInterval(_swapInterval);

    // Setup the projection surface
#ifdef DEBUG
    glGetError();
#endif

    _screen = make_shared<Object>();
    _screen->setAttribute("fill", {"window"});
    GeometryPtr virtualScreen = make_shared<Geometry>();
    _screen->addGeometry(virtualScreen);

    _screenGui = make_shared<Object>();
    _screenGui->setAttribute("fill", {"window"});
    virtualScreen = make_shared<Geometry>();
    _screenGui->addGeometry(virtualScreen);

#ifdef DEBUG
    GLenum error = glGetError();
    if (error)
        Log::get() << Log::WARNING << __FUNCTION__ << " - Error while creating the projection surface: " << error << Log::endl;
#endif

    _window->releaseContext();

#ifdef DEBUG
    return error == 0 ? true : false;
#else
    return true;
#endif
}

/*************/
void Window::setWindowDecoration(bool hasDecoration)
{
    if (_screenId != -1)
        return;

    glfwWindowHint(GLFW_VISIBLE, true);
    glfwWindowHint(GLFW_RESIZABLE, hasDecoration);
    glfwWindowHint(GLFW_DECORATED, hasDecoration);
    GLFWwindow* window;
    window = glfwCreateWindow(_windowRect[2], _windowRect[3], ("Splash::" + _name).c_str(), 0, _window->getMainWindow());

    // Reset hints to default ones
    glfwWindowHint(GLFW_RESIZABLE, true);
    glfwWindowHint(GLFW_DECORATED, true);

    if (!window)
    {
        Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - Unable to update window " << _name << Log::endl;
        return;
    }

    _window = move(make_shared<GlWindow>(window, _window->getMainWindow()));
    updateSwapInterval();
    setupRenderFBO();
    setupReadFBO();

    setEventsCallbacks();
    showCursor(false);

    return;
}

/*************/
void Window::updateSwapInterval()
{
    if (!_window->setAsCurrentContext()) 
    	 Log::get() << Log::WARNING << "Window::" << __FUNCTION__ << " - A previous context has not been released." << Log::endl;;

    glfwSwapInterval(_swapInterval);

    _window->releaseContext();
}

/*************/
void Window::updateWindowShape()
{
    if (_screenId == -1)
    {
        glfwSetWindowPos(_window->get(), _windowRect[0], _windowRect[1]);
        glfwSetWindowSize(_window->get(), _windowRect[2], _windowRect[3]);
    }
}

/*************/
void Window::registerAttributes()
{
    _attribFunctions["fullscreen"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        switchFullscreen(args[0].asInt());
        return true;
    }, [&]() -> Values {
        return {_screenId};
    });

    _attribFunctions["decorated"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        _withDecoration = args[0].asInt() == 0 ? false : true;
        setWindowDecoration(_withDecoration);
        updateWindowShape();
        return true;
    }, [&]() -> Values {
        if (_screenId != -1)
            return Values();
        else
            return {(int)_withDecoration};
    });

    _attribFunctions["srgb"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        if (args[0].asInt() != 0)
            _srgb = true;
        else
            _srgb = false;
        return true;
    }, [&]() -> Values {
        return {_srgb};
    });

    _attribFunctions["gamma"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        _gammaCorrection = args[0].asFloat();
        return true;
    }, [&]() -> Values {
        return {_gammaCorrection};
    });

    // Attribute to configure the placement of the various texture input
    _attribFunctions["layout"] = AttributeFunctor([&](const Values& args) {
        if (args.size() < 1)
            return false;
        _layout = args;
        return true;
    }, [&]() {
        return _layout;
    });

    _attribFunctions["position"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 2)
            return false;
        _windowRect[0] = args[0].asInt();
        _windowRect[1] = args[1].asInt();
        updateWindowShape();
        return true;
    }, [&]() -> Values {
        if (_screenId != -1)
            return {};
        else
            return {_windowRect[0], _windowRect[1]};
    });

    _attribFunctions["size"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 2)
            return false;
        _windowRect[2] = args[0].asInt();
        _windowRect[3] = args[1].asInt();
        updateWindowShape();
        return true;
    }, [&]() -> Values {
        if (_screenId != -1)
            return {};
        else
            return {_windowRect[2], _windowRect[3]};
    });

    _attribFunctions["swapInterval"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        _swapInterval = max(-1, args[0].asInt());
        updateSwapInterval();
        return true;
    });

    _attribFunctions["swapTest"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 1)
            return false;
        _swapSynchronizationTesting = args[0].asInt();
        return true;
    });

    _attribFunctions["swapTestColor"] = AttributeFunctor([&](const Values& args) {
        if (args.size() != 4)
            return false;
        _swapSynchronizationColor = glm::vec4(args[0].asFloat(), args[1].asFloat(), args[2].asFloat(), args[3].asFloat());
        return true;
    });
}

} // end of namespace
