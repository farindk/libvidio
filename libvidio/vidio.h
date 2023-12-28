/*
 * VidIO library
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libvidio.
 *
 * libvidio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libvidio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libvidio.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBVIDIO_VIDIO_H
#define LIBVIDIO_VIDIO_H

#if defined(_MSC_VER) && !defined(LIBVIDIO_STATIC_BUILD)
#ifdef LIBVIDIO_EXPORTS
#define LIBVIDIO_API __declspec(dllexport)
#else
#define LIBVIDIO_API __declspec(dllimport)
#endif
#elif defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
#ifdef LIBVIDIO_EXPORTS
#define LIBVIDIO_API __attribute__((__visibility__("default")))
#else
#define LIBVIDIO_API
#endif
#else
#define LIBVIDIO_API
#endif


extern "C" {

LIBVIDIO_API const char* vidio_get_version_name();
};

#endif //LIBVIDIO_VIDIO_H
