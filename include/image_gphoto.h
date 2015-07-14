/*
 * Copyright (C) 2015 Emmanuel Durand
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
 * @image_gphoto.h
 * The Image_GPhoto class
 * This code was inspired by Darktable tethering implementation
 */

#ifndef SPLASH_IMAGE_GPHOTO_H
#define SPLASH_IMAGE_GPHOTO_H

#include <gphoto2/gphoto2.h>

#include "config.h"

#include "image.h"

namespace oiio = OIIO_NAMESPACE;

typedef Camera GpCamera;

namespace Splash
{

class Image_GPhoto : public Image
{
    public:
        /**
         * Constructor
         */
        Image_GPhoto();
        Image_GPhoto(std::string cameraName);

        /**
         * Destructor
         */
        ~Image_GPhoto();

        /**
         * No copy constructor, only move
         */
        Image_GPhoto(const Image_GPhoto&) = delete;
        Image_GPhoto& operator=(const Image_GPhoto&) = delete;

        /**
         * Capture a new photo
         */
        bool capture();

        /**
         * Set the camera to read from
         */
        bool read(const std::string& cameraName);

    private:
        struct GPhotoCamera
        {
            std::string model;
            std::string port;
            GpCamera* cam {nullptr};
            CameraWidget* configuration {nullptr};

            bool canTether {false};
            bool canConfig {false};
            bool canImport {false};

            std::vector<std::string> shutterspeeds;
            std::vector<std::string> apertures;
            std::vector<std::string> isos;
        };

        std::recursive_mutex _gpMutex;
        GPContext* _gpContext {nullptr};
        CameraAbilitiesList* _gpCams {nullptr};
        GPPortInfoList* _gpPorts {nullptr};

        std::vector<GPhotoCamera> _cameras;
        int _selectedCameraIndex {-1};

        /**
         * Detect connected cameras
         */
        void detectCameras();

        /**
         * Various commands sent to the camera
         */
        bool doSetProperty(std::string name, std::string value);
        bool doGetProperty(std::string name, std::string& value);

        /**
         * Conversion between float and shutterspeed (as a string)
         */
        float getFloatFromShutterspeedString(std::string speed);
        std::string getShutterspeedStringFromFloat(float duration);

        /**
         * Initialize the whole gphoto context
         */
        void init();

        /**
         * Initialize the given camera
         */
        bool initCamera(GPhotoCamera& camera);
        void initCameraProperty(GPhotoCamera& camera, std::string property, std::vector<std::string>& values);

        /**
         * Release the given camera
         */
        void releaseCamera(GPhotoCamera& camera);

        /**
         * Register new functors to modify attributes
         */
        void registerAttributes();
};

typedef std::shared_ptr<Image_GPhoto> Image_GPhotoPtr;

} // end of namespace

#endif // SPLASH_IMAGE_GPHOTO_H
