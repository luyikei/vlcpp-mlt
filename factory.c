/*****************************************************************************
 * factory.c: factory for vlcpp-mlt plugins
 *****************************************************************************
 * Copyright (C) 2008-2016 Yikei Lu
 *
 * Authors: Yikei Lu    <luyikei.qmltu@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <string.h>
#include <limits.h>
#include <framework/mlt.h>

extern mlt_consumer consumer_vlc_init( mlt_profile profile, mlt_service_type type, const char* id, char* arg );
extern mlt_producer producer_vlc_init( mlt_profile profile, mlt_service_type type , const char* id , char* arg );


static mlt_properties metadata( mlt_service_type type, const char *id, char* data )
{
    char file[ PATH_MAX ];
    snprintf( file, PATH_MAX, "%s/vlc/%s", mlt_environment( "MLT_DATA" ), (char*) data );
    return mlt_properties_parse_yaml( file );
}

MLT_REPOSITORY
{
    MLT_REGISTER( consumer_type, "vlc", consumer_vlc_init );
    MLT_REGISTER_METADATA( consumer_type, "vlc", metadata, "consumer_vlc.yml" );
    MLT_REGISTER( producer_type, "vlc", producer_vlc_init );
    MLT_REGISTER_METADATA( producer_type, "vlc", metadata, "producer_vlc.yml" );
}
