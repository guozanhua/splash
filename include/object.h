/*
 * Copyright (C) 2013 Emmanuel Durand
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
 * @object.h
 * The Object class
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <config.h>

#include <memory>
#include <vector>

#include "shader.h"
#include "texture.h"
#include "geometry.h"

namespace Splash {

class Object {
    public:
        /**
         * Constructor
         */
        Object();

        /**
         * Destructor
         */
        ~Object();

        /**
         * Activate this object for rendering
         */
        void activate();

        /**
         * Get the shader
         */
        ShaderPtr getShader() const {return _shader;}

        /**
         * Add a geometry to this object
         */
        void addGeometry(GeometryPtr geometry) {_geometries.push_back(geometry);}

        /**
         * Add a texture to this object
         */
        void addTexture(TexturePtr texture) {_textures.push_back(texture);}

    private:
        ShaderPtr _shader;
        std::vector<TexturePtr> _textures;
        std::vector<GeometryPtr> _geometries;
};

typedef std::shared_ptr<Object> ObjectPtr;

} // end of namespace

#endif // OBJECT_H
