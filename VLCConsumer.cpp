/*****************************************************************************
 * VLCConsumer.cpp: Implementation for vlc consumer
 *****************************************************************************
 * Copyright (C) 2008-2016 Yikei Lu
 *
 * Authors: Yikei Lu    <luyikei.qmltu@gmail.com>
 * Thanks:  Pawel Golinski <golpaw1@gmail.com>
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



#include <string>
#include <deque>
#include <vector>
#include <sys/time.h>
#include <memory>
#include <mutex>

#include <mlt++/MltConsumer.h>

#include <vlcpp/vlc.hpp>

#include "common.hpp"

class VLCConsumer
{
public:
    static void onPropertyChanged( void*, VLCConsumer* self, const char* id )
    {
        if ( self == nullptr )
            return;

        if ( strcmp( id, "volume" ) == 0 )
        {
            self->m_mediaPlayer.setVolume( self->m_parent->get_double( "volume" ) * 100 );
        }
    }

    VLCConsumer( mlt_profile profile )
        : m_lastAudioPts( 0 )
        , m_lastVideoPts( 0 )
    {
        mlt_consumer parent = new mlt_consumer_s;
        mlt_consumer_init( parent, this, profile );
        m_parent.reset( new Mlt::Consumer( parent ) );
        m_parent->listen( "property-changed", this, ( mlt_listener ) onPropertyChanged );
        m_parent->dec_ref();
        auto mlt_parent = m_parent->get_consumer();

        m_parent->set( "input_image_format", mlt_image_yuv422 );
        m_parent->set( "input_audio_format", mlt_audio_s16 );
        m_parent->set( "buffer", 1 );

        mlt_parent->start = consumer_start;
        mlt_parent->stop = consumer_stop;
        mlt_parent->close = consumer_close;
        mlt_parent->is_stopped = consumer_is_stopped;
        mlt_parent->purge = consumer_purge;

        resetMedia();
    }

    void resetMedia()
    {
        char videoString[512];
        char inputSlave[256];
        char audioParameters[256];
        sprintf( videoString, "width=%i:height=%i:dar=%i/%i:fps=%i/%i:cookie=0:codec=%s:cat=2:caching=0",
                 m_parent->get_int( "width" ),
                 m_parent->get_int( "height" ),
                 m_parent->get_int( "sample_aspect_num" ),
                 m_parent->get_int( "sample_aspect_den" ),
                 m_parent->get_int( "frame_rate_num" ),
                 m_parent->get_int( "frame_rate_den" ),
                 "YUY2" );
        sprintf( audioParameters, "cookie=1:cat=1:codec=s16l:samplerate=%u:channels=%u:caching=0",
                 m_parent->get_int( "frequency" ),
                 m_parent->get_int( "channels" ) );
        strcpy( inputSlave, ":input-slave=imem://" );
        strcat( inputSlave, audioParameters );
        m_media = VLC::Media( instance, std::string( "imem://" ) + videoString,
                              VLC::Media::FromType::FromLocation );
        m_media.addOption( inputSlave );

        char        buffer[64];
        sprintf( buffer, "imem-get=%p", imem_get );
        m_media.addOption( buffer );
        sprintf( buffer, ":imem-release=%p", imem_release );
        m_media.addOption( buffer );
        sprintf( buffer, ":imem-data=%p", this );
        m_media.addOption( buffer );

        m_mediaPlayer = VLC::MediaPlayer( m_media );
    }

    mlt_consumer consumer()
    {
        return m_parent->get_consumer();
    }

    void setXWindow( int64_t id )
    {
        m_mediaPlayer.setXwindow( id );
    }

    bool start()
    {
        setXWindow( m_parent->get_int64( "window_id" ) );
        return m_mediaPlayer.play();
    }

    bool stop()
    {
        m_mediaPlayer.stop();
        clean();
        return true;
    }

    bool isStopped()
    {
        return !m_mediaPlayer.isPlaying();
    }

    void setPause( bool val )
    {
        m_mediaPlayer.setPause( val );
    }

    void purge()
    {
        m_audioFrames.clear();
        m_videoFrames.clear();
    }

    void clean()
    {
        m_lastAudioPts = 0;
        m_lastVideoPts = 0;
        purge();
        resetMedia();
    }

    ~VLCConsumer() = default;

    static const uint8_t     VideoCookie = '0';
    static const uint8_t     AudioCookie = '1';

private:

    static int imem_get( void* data, const char* cookie, int64_t* dts, int64_t* pts,
                        unsigned* flags, size_t* bufferSize, void** buffer )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( data );
        std::unique_lock<std::mutex> lck( vlcConsumer->m_safeLock );

        if ( cookie[0] == VLCConsumer::AudioCookie )
        {
            std::shared_ptr<Mlt::Frame> frame;

            bool cleanup;
            if ( ( cleanup = vlcConsumer->m_audioFrames.size() > 0 ) )
            {
                frame = vlcConsumer->m_audioFrames.front();
                vlcConsumer->m_audioFrames.pop_front();
            }
            else
            {
                frame = std::make_shared<Mlt::Frame>( mlt_consumer_rt_frame( vlcConsumer->m_parent->get_consumer() ) );
                frame->dec_ref();
            }

            mlt_audio_format audioFormat = mlt_audio_s16;
            int frequency = frame->get_int( "audio_frequency" );
            int channels = frame->get_int( "audio_channels" );
            int samples = mlt_sample_calculator(
                vlcConsumer->m_parent->get_double( "fps" ),
                frequency,
                mlt_frame_original_position( frame->get_frame() )
            );

            *buffer = frame->get_audio( audioFormat, frequency, channels, samples );
            *bufferSize = mlt_audio_format_size( audioFormat, samples, channels );
            double ptsDiff = ( double ) samples / frequency * 1000000.0 + 0.5;


            *pts = vlcConsumer->m_lastAudioPts + ptsDiff;
            mlt_log_debug( vlcConsumer->consumer(), "%ld", *pts );
            vlcConsumer->m_lastAudioPts = *pts;
            *dts = *pts;

            if ( cleanup == true )
                vlcConsumer->m_lastAudioFrame = frame;
            else
                vlcConsumer->m_videoFrames.push_back( frame );
        }
        else if ( cookie[0] == VLCConsumer::VideoCookie )
        {
            std::shared_ptr<Mlt::Frame> frame;

            bool cleanup;
            if ( ( cleanup = vlcConsumer->m_videoFrames.size() > 0 ) )
            {
                frame = vlcConsumer->m_videoFrames.front();
                vlcConsumer->m_videoFrames.pop_front();
            }
            else
            {
                frame = std::make_shared<Mlt::Frame>( mlt_consumer_rt_frame( vlcConsumer->m_parent->get_consumer() ) );
                frame->dec_ref();
            }

            mlt_image_format videoFormat = mlt_image_yuv422;
            int width = frame->get_int( "width" );
            int height = frame->get_int( "height" );

            *buffer = frame->get_image( videoFormat, width, height, 0 );
            *bufferSize = mlt_image_format_size( videoFormat, width, height, NULL );
            double ptsDiff = 1.0 / vlcConsumer->m_parent->get_double( "fps" ) * 1000000.0 + 0.5;

            *pts = vlcConsumer->m_lastVideoPts + ptsDiff;
            vlcConsumer->m_lastVideoPts = *pts;
            *dts = *pts;

            if ( cleanup == true )
                vlcConsumer->m_lastVideoFrame = frame;
            else
                vlcConsumer->m_audioFrames.push_back( frame );
        }
        else
            return 1;

        return 0;
    }

    static void imem_release( void* data, const char* cookie, size_t buffSize, void* buffer )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( data );
        std::unique_lock<std::mutex> lck( vlcConsumer->m_safeLock );

        if ( cookie[0] == VLCConsumer::AudioCookie )
        {
            if ( vlcConsumer->m_lastAudioFrame == nullptr )
                return;
            mlt_events_fire( vlcConsumer->m_parent->get_properties(),
                            "consumer-frame-show", vlcConsumer->m_lastAudioFrame->get_frame(), NULL );
            vlcConsumer->m_lastAudioFrame = nullptr;
        }
        else if ( cookie[0] == VLCConsumer::VideoCookie )
        {
            if ( vlcConsumer->m_lastVideoFrame == nullptr )
                return;
            mlt_events_fire( vlcConsumer->m_parent->get_properties(),
                            "consumer-frame-show", vlcConsumer->m_lastVideoFrame->get_frame(), NULL );
            vlcConsumer->m_lastVideoFrame = nullptr;
        }
    }

    static int consumer_start( mlt_consumer parent )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( parent->child );
        return !( vlcConsumer->start() );
    }

    static int consumer_stop( mlt_consumer parent )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( parent->child );
        return !( vlcConsumer->stop() );
    }

    static int consumer_is_stopped( mlt_consumer parent )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( parent->child );
        return vlcConsumer->isStopped();
    }

    static void consumer_purge( mlt_consumer parent )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( parent->child );
        vlcConsumer->purge();
    }

    static void consumer_close( mlt_consumer parent )
    {
        auto vlcConsumer = reinterpret_cast<VLCConsumer*>( parent->child );
        delete vlcConsumer;
    }

    std::unique_ptr<Mlt::Consumer>      m_parent;

    VLC::Media          m_media;
    VLC::MediaPlayer    m_mediaPlayer;

    std::mutex          m_safeLock;

    std::shared_ptr<Mlt::Frame>         m_lastAudioFrame;
    std::shared_ptr<Mlt::Frame>         m_lastVideoFrame;

    // This is a trick to avoid calling mlt_consumer_rt_frame twice ( for audio and video ) for one frame.
    std::deque<std::shared_ptr<Mlt::Frame>> m_audioFrames;
    std::deque<std::shared_ptr<Mlt::Frame>> m_videoFrames;

    int64_t             m_lastAudioPts;
    int64_t             m_lastVideoPts;

};

extern "C" mlt_consumer consumer_vlc_init_CXX( mlt_profile profile, mlt_service_type type , const char* id , char* arg )
{
    return ( new VLCConsumer( profile ) )->consumer();
}

