/*
 * Copyright (C) 2014 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @coretypes.h
 * A few, mostly basic, types
 */

//#define GL_GLEXT_PROTOTYPES
//#define GLX_GLXEXT_PROTOTYPES

#include "config.h"

#define SPLASH
#define SPLASH_GL_DEBUG true
#define SPLASH_SAMPLES 0

#define SPLASH_ALL_PAIRS "__ALL__"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <execinfo.h>
#include <ostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "threadpool.h"

#ifndef SPLASH_CORETYPES_H
#define SPLASH_CORETYPES_H

#define PRINT_FUNCTION_LINE std::cout << "------> " << __FUNCTION__ << "::" << __LINE__ << std::endl;
#define PRINT_CALL_STACK \
{ \
    int j, nptrs; \
    int size = 100; \
    void* buffers[size]; \
    char** strings; \
    \
    nptrs = backtrace(buffers, size); \
    strings = backtrace_symbols(buffers, nptrs); \
    for (j = 0; j < nptrs; ++j) \
        std::cout << strings[j] << endl; \
    \
    free(strings); \
}

namespace Splash
{

/*************/
struct SerializedObject
{
    /**
     * Constructors
     */
    SerializedObject() {}

    SerializedObject(int size)
    {
        _data.resize(size);
    }

    SerializedObject(char* start, char* end)
    {
        _data = std::vector<char>(start, end);
    }

    /**
     * Get the pointer to the data
     */
    char* data()
    {
        return _data.data();
    }

    /**
     * Return the size of the data
     */
    std::size_t size()
    {
        return _data.size();
    }

    /**
     * Modify the size of the data
     */
    void resize(size_t s)
    {
        _data.resize(s);
    }

    /**
     * Attributes
     */
    std::vector<char> _data;
};

/*************/
class GlWindow
{
    public:
        /**
         * Constructors
         */
        GlWindow(GLFWwindow* w, GLFWwindow* mainWindow)
        {
            _window = w;
            _mainWindow = mainWindow;
        }

        /**
         * Destructor
         */
        ~GlWindow()
        {
            if (_window != nullptr)
                glfwDestroyWindow(_window);
        }

        /**
         * Get the pointer to the GLFW window
         */
        GLFWwindow* get() const {return _window;}

        /**
         * Get the pointer to the main GLFW window
         */
        GLFWwindow* getMainWindow() const {return _mainWindow;}

        /**
         * Set the context of this window as current
         */
        bool setAsCurrentContext() const 
        {
            _previousWindow = glfwGetCurrentContext();
            if (_previousWindow == _window)
                return true;
            _mutex.lock();
            glfwMakeContextCurrent(_window);
            return true;
        }

        /**
         * Release the context
         */
        void releaseContext() const
        {
            if (_window == _previousWindow)
                _previousWindow = nullptr;
            else if (glfwGetCurrentContext() == _window)
            {
                if (_previousWindow == nullptr)
                    glfwMakeContextCurrent(NULL);
                else
                {
                    glfwMakeContextCurrent(_previousWindow);
                    _previousWindow = nullptr;
                }
                _mutex.unlock();
            }
        }

    private:
        mutable std::mutex _mutex;
        mutable GLFWwindow* _previousWindow {nullptr};
        GLFWwindow* _window {nullptr};
        GLFWwindow* _mainWindow {nullptr};
};

typedef std::shared_ptr<GlWindow> GlWindowPtr;

struct Value;
typedef std::deque<Value> Values;

/*************/
struct Value
{
    public:
        enum Type
        {
            i = 0,
            l,
            f,
            s,
            v
        };

        Value() {_i = 0; _type = Type::i;}
        Value(int v) {_i = v; _type = Type::i;}
        Value(int64_t v) {_l = v; _type = Type::l;}
        Value(float v) {_f = v; _type = Type::f;}
        Value(double v) {_f = (float)v; _type = Type::f;}
        Value(std::string v) {_s = v; _type = Type::s;}
        Value(const char* c) {_s = std::string(c); _type = Type::s;}
        Value(Values v)
        {
            if (!_v)
                _v = new Values();
            *_v = v;
            _type = Type::v;
        }

        Value(const Value& v)
        {
            operator=(v);
        }

        Value& operator=(const Value& v)
        {
            if (this != &v)
            {
                _type = v._type;
                _i = v._i;
                _l = v._l;
                _f = v._f;
                _s = v._s;

                if (!_v && v._v)
                {
                    _v = new Values();
                    *_v = *v._v;
                }
            }

            return *this;
        }

        ~Value()
        {
            if (_v)
                delete _v;
        }

        template<class InputIt>
        Value(InputIt first, InputIt last)
        {
            _type = Type::v;
            if (!_v)
                _v = new Values();
            _v->clear();

            auto it = first;
            while (it != last)
            {
                _v->push_back(Value(*it));
                ++it;
            }
        }

        bool operator==(Value v) const
        {
            if (_type != v._type)
                return false;
            else if (_type == Type::i)
                return _i == v._i;
            else if (_type == Type::l)
                return _l == v._l;
            else if (_type == Type::f)
                return _f == v._f;
            else if (_type == Type::s)
                return _s == v._s;
            else if (_type == Type::v)
            {
                if (_v->size() != v._v->size())
                    return false;
                bool isEqual = true;
                for (int i = 0; i < _v->size(); ++i)
                    isEqual &= (_v->at(i) == v._v->at(i));
                return isEqual;
            }
            else
                return false;
        }

        Value& operator[](int index)
        {
            if (_type != Type::v)
                return *this;
            else
                return _v->at(index);
        }

        int asInt() const
        {
            if (_type == Type::i)
                return _i;
            else if (_type == Type::l)
                return (int)_l;
            else if (_type == Type::f)
                return (int)_f;
            else if (_type == Type::s)
                try {return std::stoi(_s);}
                catch (...) {return 0;}
            else
                return 0;
        }

        int64_t asLong() const
        {
            if (_type == Type::i)
                return (int64_t)_i;
            else if (_type == Type::l)
                return _l;
            else if (_type == Type::f)
                return (int64_t)_f;
            else if (_type == Type::s)
                try {return std::stoi(_s);}
                catch (...) {return 0;}
            else
                return 0;
        }

        float asFloat() const
        {
            if (_type == Type::i)
                return (float)_i;
            else if (_type == Type::l)
                return (float)_l;
            else if (_type == Type::f)
                return _f;
            else if (_type == Type::s)
                try {return std::stof(_s);}
                catch (...) {return 0.f;}
            else
                return 0.f;
        }

        std::string asString() const
        {
            if (_type == Type::i)
                try {return std::to_string(_i);}
                catch (...) {return std::string();}
            else if (_type == Type::l)
                try {return std::to_string(_l);}
                catch (...) {return std::string();}
            else if (_type == Type::f)
                try {return std::to_string(_f);}
                catch (...) {return std::string();}
            else if (_type == Type::s)
                return _s;
            else
                return "";
        }

        Values asValues() const
        {
            if (_type == Type::i)
                return {_i};
            else if (_type == Type::l)
                return {_l};
            else if (_type == Type::f)
                return {_f};
            else if (_type == Type::s)
                return {_s};
            else if (_type == Type::v)
                return *_v;
            else
                return {};
        }

        void* data()
        {
            if (_type == Type::i)
                return (void*)&_i;
            else if (_type == Type::l)
                return (void*)&_l;
            else if (_type == Type::f)
                return (void*)&_f;
            else if (_type == Type::s)
                return (void*)_s.c_str();
            else
                return nullptr;
        }

        Type getType() const {return _type;}
        
        int size()
        {
            if (_type == Type::i)
                return sizeof(_i);
            else if (_type == Type::l)
                return sizeof(_l);
            else if (_type == Type::f)
                return sizeof(_f);
            else if (_type == Type::s)
                return _s.size();
            else
                return 0;
        }

    private:
        Type _type;
        int _i {0};
        int64_t _l {0};
        float _f {0.f};
        std::string _s {""};
        Values* _v {nullptr};
};

/*************/
// OnScopeExit, taken from Switcher (https://github.com/nicobou/switcher)
template <typename F>
class ScopeGuard
{
    public:
        explicit ScopeGuard(F &&f) :
            f_(std::move(f)) {}
        ~ScopeGuard()
        {
            f_();
        }
    private:
        F f_;
};

enum class ScopeGuardOnExit { };
template <typename F>
ScopeGuard<F> operator+(ScopeGuardOnExit, F&& f)
{
    return ScopeGuard<F>(std::forward<F>(f));
}

#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#define OnScopeExit auto CONCATENATE(on_scope_exit_var, __LINE__) = ScopeGuardOnExit() + [&]()

} // end of namespace

#endif // SPLASH_CORETYPES_H
