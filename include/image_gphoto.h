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
 * blobserver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with blobserver.  If not, see <http://www.gnu.org/licenses/>.
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

        /**
         * Destructor
         */
        ~Image_GPhoto();

        /**
         * No copy constructor, only move
         */
        Image_GPhoto(const Image_GPhoto&) = delete;
        Image_GPhoto& operator=(const Image_GPhoto&) = delete;

        Image_GPhoto(Image_GPhoto&& g) noexcept
        {
            *this = std::move(g);
        }

        Image_GPhoto& operator=(Image_GPhoto&& g) noexcept
        {
            if (this != &g)
            {
            }
            return *this;
        }

        /**
         * Capture a new photo
         */
        void capture();

        /**
         * Set the path to read from
         */
        bool read(const std::string& filename);

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
         * Initialize the whole gphoto context
         */
        void init();

        /**
         * Initialize the given camera
         */
        bool initCamera(GPhotoCamera& camera);

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
