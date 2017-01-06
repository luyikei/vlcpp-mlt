/*****************************************************************************
 * factory.c: factory for vlcpp-mlt plugins
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
#include <mutex>

#include <sys/time.h>

#include <mlt++/MltProfile.h>
#include <mlt++/MltProducer.h>

#include <vlcpp/vlc.hpp>

class VLCProducer
{
public:
    VLCProducer( mlt_profile profile, char* file )
        : m_parent( nullptr )
        , m_instance( nullptr )
        , m_media( nullptr )
        , m_mediaPlayer( nullptr )
        , m_audioIndex( -1 )
        , m_videoIndex( -1 )
        , m_lastPosition( -1 )
    {
        if ( !file )
            return;
            
        mlt_producer parent = ( mlt_producer ) calloc( 1, sizeof( struct mlt_producer_s ) );
        if ( parent && mlt_producer_init( parent, this ) == 0 )
        {
            m_parent = new Mlt::Producer( parent );
            m_parent->dec_ref();
            
            parent->get_frame = producer_get_frame;
            parent->close = ( mlt_destructor ) producer_close;
            
            m_parent->set_lcnumeric( "C" );
            m_parent->set( "resource", file );
            m_parent->set( "_profile", ( void* ) profile, 0, NULL, NULL );
            
            const char * const argv[] = {
                "--no-skip-frames",
                "dummy",
                "--text-renderer",
                "--no-sub-autodetect-file",
                "--no-disable-screensaver",
                NULL,
            };

            m_instance = new VLC::Instance( 5, argv );
            m_media = new VLC::Media( *m_instance, std::string( "" ) + file, VLC::Media::FromType::FromPath );
            m_media->parseWithOptions( VLC::Media::ParseFlags::Local, 3000 );
            while ( m_media->parsedStatus() != VLC::Media::ParsedStatus::Done );
            if ( m_media->parsedStatus() == VLC::Media::ParsedStatus::Done )
            {
                auto tracks = m_media->tracks();
                m_parent->set( "meta.media.nb_streams", ( int ) tracks.size() );
                int i = 0;
                char key[200];
                for ( const auto& track : tracks )
                {
                    if ( track.type() == VLC::MediaTrack::Video )
                    {
                        if ( m_videoIndex == -1 )
                            m_videoIndex = i;
                        
                        snprintf( key, sizeof(key), "meta.media.%d.stream.type", i );
                        m_parent->set( key, "video" );
                        
                        snprintf( key, sizeof(key), "meta.media.%d.stream.frame_rate", i );
                        m_parent->set( key, ( double ) track.fpsNum() / track.fpsDen() );
                        snprintf( key, sizeof(key), "meta.media.%d.stream.frame_rate_num", i );
                        m_parent->set( key, ( int64_t ) track.fpsNum() );
                        snprintf( key, sizeof(key), "meta.media.%d.stream.frame_rate_den", i );
                        m_parent->set( key, ( int64_t ) track.fpsDen() );
                        
                        snprintf( key, sizeof(key), "meta.media.%d.codec.frame_rate", i );
                        m_parent->set( key, ( double ) track.fpsNum() / track.fpsDen() );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.frame_rate_num", i );
                        m_parent->set( key, ( int64_t ) track.fpsNum() );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.frame_rate_den", i );
                        m_parent->set( key, ( int64_t ) track.fpsDen() );
                        
                        snprintf( key, sizeof(key), "meta.media.%d.stream.sample_aspect_ratio", i );
                        m_parent->set( key, ( double ) track.sarNum() / track.sarDen() );
                        snprintf( key, sizeof(key), "meta.media.%d.stream.frame_rate_num", i );
                        m_parent->set( key, ( int64_t ) track.sarNum() );
                        snprintf( key, sizeof(key), "meta.media.%d.stream.frame_rate_den", i );
                        m_parent->set( key, ( int64_t ) track.sarDen() );
                        
                        snprintf( key, sizeof(key), "meta.media.%d.codec.sample_aspect_ratio", i );
                        m_parent->set( key, ( double ) track.sarNum() / track.sarDen() );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.frame_rate_num", i );
                        m_parent->set( key, ( int64_t ) track.sarNum() );
                        snprintf( key, sizeof(key), "meta.media.%d.codex.frame_rate_den", i );
                        m_parent->set( key, ( int64_t ) track.sarDen() );
                        
                        
                        snprintf( key, sizeof(key), "meta.media.%d.codec.width", i );
                        m_parent->set( key, ( int64_t ) track.width() );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.height", i );
                        m_parent->set( key, ( int64_t ) track.height() );
                    }
                    else if ( track.type() == VLC::MediaTrack::Audio )
                    {
                        if ( m_audioIndex == -1 )
                            m_audioIndex = i;
                        
                        snprintf( key, sizeof(key), "meta.media.%d.stream.type", i );
                        m_parent->set( key, "audio" );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.sample_rate", i );
                        m_parent->set( key, ( int64_t ) track.rate() );
                        snprintf( key, sizeof(key), "meta.media.%d.codec.channels", i );
                        m_parent->set( key, ( int64_t ) track.channels() );
                    }
                        
                    snprintf( key, sizeof(key), "meta.media.%d.codec.fourcc", i );
                    m_parent->set( key, ( int64_t ) track.codec() );
                    snprintf( key, sizeof(key), "meta.media.%d.codec.original_fourcc", i );
                    m_parent->set( key, ( int64_t ) track.originalFourCC() );
                    snprintf( key, sizeof(key), "meta.media.%d.codec.bit_rate", i );
                    m_parent->set( key, ( int64_t ) track.bitrate() );
                    i++;
                }
                
                if ( m_videoIndex != -1 )
                {
                    m_parent->set( "width", ( int64_t ) tracks[m_videoIndex].width() );
                    m_parent->set( "meta.media.width", ( int64_t ) tracks[m_videoIndex].width() );
                    m_parent->set( "height", ( int64_t ) tracks[m_videoIndex].height() );
                    m_parent->set( "meta.media.height", ( int64_t ) tracks[m_videoIndex].height() );
                    m_parent->set( "height", ( int64_t ) tracks[m_videoIndex].height() );
                    m_parent->set( "meta.media.sample_aspect_num", ( int64_t ) tracks[m_videoIndex].sarNum() );
                    m_parent->set( "meta.media.sample_aspect_den", ( int64_t ) tracks[m_videoIndex].sarDen() );
                    m_parent->set( "aspect_ratio", ( double ) tracks[m_videoIndex].sarNum() / tracks[m_videoIndex].sarDen() );
                    m_parent->set( "meta.media.frame_rate_num", ( int64_t ) tracks[m_videoIndex].fpsNum() );
                    m_parent->set( "meta.media.frame_rate_den", ( int64_t ) tracks[m_videoIndex].fpsDen() );
                    m_parent->set( "frame_rate", ( double ) tracks[m_videoIndex].fpsNum() / tracks[m_videoIndex].fpsDen() );
                    m_parent->set( "length", ( int ) ( m_media->duration() * mlt_profile_fps( ( mlt_profile ) m_parent->get_data( "_profile" ) ) ) / 1000 + 0.5 );
                    m_parent->set( "out", ( int ) m_parent->get_int( "length" ) - 1 );
                }
                
                if ( m_audioIndex != -1 )
                {
                    m_parent->set( "meta.media.sample_rate", ( int64_t ) tracks[m_audioIndex].rate() );
                    m_parent->set( "meta.media.channels", ( int64_t ) tracks[m_audioIndex].channels() );
                    m_parent->set( "sample_rate", ( int64_t ) tracks[m_audioIndex].rate() );
                    m_parent->set( "channels", ( int64_t ) tracks[m_audioIndex].channels() );
                }
                
            }
            
            char smem_options[ 1000 ];
            sprintf( smem_options,
                     ":sout=#transcode{"
                     "vcodec=%s,"
                     "fps=%d/%d,"\
                     "acodec=%s,"
                     "}:smem{"
                     "time-sync,"
                     "no-sout-smem-time-sync,"
                     "audio-prerender-callback=%" PRIdPTR ","
                     "audio-postrender-callback=%" PRIdPTR ","
                     "video-prerender-callback=%" PRIdPTR ","
                     "video-postrender-callback=%" PRIdPTR ","
                     "audio-data=%" PRIdPTR ","
                     "video-data=%" PRIdPTR ","
                     "}",
                     "YUY2",
                     profile->frame_rate_num,
                     profile->frame_rate_den,\
                     "s16l",
                     ( intptr_t ) &audio_lock,
                     ( intptr_t ) &audio_unlock,
                     ( intptr_t ) &video_lock,
                     ( intptr_t ) &video_unlock,
                     ( intptr_t ) this,
                     ( intptr_t ) this
            );
            
            m_media->addOption( smem_options );
            m_mediaPlayer = new VLC::MediaPlayer( *m_media );
            m_mediaPlayer->play();
        }
    }
    
    mlt_producer producer()
    {
        return m_parent->get_producer();
    }
    
    ~VLCProducer()
    {
        if ( m_mediaPlayer != nullptr )
            m_mediaPlayer->stop();
        delete m_mediaPlayer;
        delete m_media;
        delete m_instance;
        delete m_parent;
        
        clearFrames();
    }
    
private:
    
    struct Frame {
        ~Frame()
        {
            mlt_pool_release( buffer );
        }
        
        uint8_t* buffer;
        int size;
        int64_t vlcTime;
        int iterator;
    };
    
    static void audio_lock( void* data, uint8_t** buffer, size_t size )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( data );
        vlcProducer->renderLock.lock();
        
        *buffer = ( uint8_t* ) mlt_pool_alloc( size * sizeof( uint8_t ) );
    }
    
    static void audio_unlock( void* data, uint8_t* buffer, unsigned int channels,
                              unsigned int rate, unsigned int nb_samples, unsigned int bps,
                              size_t size, int64_t pts )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( data );
        
        if ( vlcProducer->m_audioFrames.size() >= 20 )
            vlcProducer->m_mediaPlayer->setPause( true );

        auto frame = new Frame;
        frame->buffer = buffer;
        frame->size = size;
        frame->iterator = 0;
        frame->vlcTime = vlcProducer->m_mediaPlayer->time();
        
        vlcProducer->m_audioFrames.push_back( frame );
        
        vlcProducer->packBufferToFrame();
        vlcProducer->renderLock.unlock();
    }
    
    static void video_lock( void* data, uint8_t** buffer, size_t size )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( data );
        vlcProducer->renderLock.lock();
        
        *buffer = ( uint8_t* ) mlt_pool_alloc( size * sizeof( uint8_t ) );
    }
    
    static void video_unlock( void* data, uint8_t* buffer, int width, int height,
                              int bpp, size_t size, int64_t pts )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( data );
        
        if ( vlcProducer->m_videoFrames.size() >= 20 )
            vlcProducer->m_mediaPlayer->setPause( true );

        auto frame = new Frame;
        frame->buffer = buffer;
        frame->size = size;
        frame->vlcTime = vlcProducer->m_mediaPlayer->time();
        
        vlcProducer->m_videoFrames.push_back( frame );
        
        vlcProducer->packBufferToFrame();
        vlcProducer->renderLock.unlock();
    }
    
    void packBufferToFrame()
    {
        int needed_samples = mlt_sample_calculator(
            mlt_profile_fps( ( mlt_profile ) m_parent->get_data( "_profile" ) ),
            m_parent->get_int64( "sample_rate" ),
            m_lastPosition );
        
        int audio_buffer_size = mlt_audio_format_size( mlt_audio_s16, needed_samples,
                                                       m_parent->get_int64( "channels" ) );
        
        if ( m_videoFrames.size() > 0 && m_audioFrames.size() > 0 &&
            ( int ) ( ( m_audioFrames.size() - 1 ) * m_audioFrames[0]->size ) >= audio_buffer_size )
        {
            auto mltFrame = new Mlt::Frame( mlt_frame_init( m_parent->get_service() ) );
            
            auto packedAudioBuffer = ( uint8_t* ) mlt_pool_alloc( audio_buffer_size );
            int iterator = 0;
            while ( iterator < audio_buffer_size )
            {
                auto frontBuffer = m_audioFrames.front();
                
                while ( frontBuffer->iterator < frontBuffer->size && iterator < audio_buffer_size )
                    packedAudioBuffer[iterator++] = frontBuffer->buffer[frontBuffer->iterator++];
                
                if ( frontBuffer->iterator == frontBuffer->size )
                {
                    m_audioFrames.pop_front();
                    delete frontBuffer;
                }
            }
            
            mlt_frame_set_audio( mltFrame->get_frame(), packedAudioBuffer, mlt_audio_s16,
                                 audio_buffer_size, ( mlt_destructor ) mlt_pool_release );
            mltFrame->set( "audio_frequency", m_parent->get_int64( "sample_rate" ) );
            mltFrame->set( "audio_channels", m_parent->get_int64( "channels" ) );
            mltFrame->set( "audio_samples", needed_samples );
            mltFrame->set( "audio_format", mlt_audio_s16 );

            auto videoFrame = m_videoFrames.front();
            
            mlt_frame_set_image( mltFrame->get_frame(), videoFrame->buffer, videoFrame->size,
                                 ( mlt_destructor ) mlt_pool_release );
            mltFrame->set( "format", mlt_image_yuv422 );
            mltFrame->set( "width", m_parent->get_int( "width" ) );
            mltFrame->set( "height", m_parent->get_int( "height" ) );
            mltFrame->set( "vlc_position", videoFrame->vlcTime * mlt_profile_fps( ( mlt_profile ) m_parent->get_data( "_profile" ) ) / 1000 + 0.5 );
            
            m_mltFrames.push_back( mltFrame );
            
            videoFrame->buffer = nullptr;
            m_videoFrames.pop_front();
            delete videoFrame;
        }
    }

    static void producer_close( mlt_producer parent )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( parent->child );
        delete vlcProducer;
    }

    static int producer_get_frame( mlt_producer producer, mlt_frame_ptr frame, int index )
    {
        auto vlcProducer = reinterpret_cast<VLCProducer*>( producer->child );
        vlcProducer->renderLock.lock();
        
        if ( vlcProducer->m_mltFrames.size() >= 20 )
            vlcProducer->m_mediaPlayer->setPause( true );
        else if ( vlcProducer->m_mltFrames.size() <= 10 )
            vlcProducer->m_mediaPlayer->setPause( false );
        
        auto posDiff = mlt_producer_position( producer ) - vlcProducer->m_lastPosition;
        bool toSeek = !( posDiff == 1 || posDiff == 2 );
        bool paused = posDiff == 0;
        if ( paused == true )
            vlcProducer->m_mediaPlayer->setPause( true );
        vlcProducer->m_lastPosition = mlt_producer_position( producer );
        
        if ( vlcProducer->m_mltFrames.size() == 0 )
        {
            *frame = mlt_frame_init( MLT_PRODUCER_SERVICE( producer ) );
            vlcProducer->renderLock.unlock();
            return -1;
        }
        else
        {
            auto mltFrame = vlcProducer->m_mltFrames.front();
            if ( paused == true )
                mltFrame->inc_ref();
            *frame = mltFrame->get_frame();
            mlt_log_error( vlcProducer->m_parent->get_service(), "pos:%d %ld\n"
            ,vlcProducer->m_lastPosition, mltFrame->get_int64( "vlc_position" ) );

            if ( paused == false )
            {
                vlcProducer->m_mltFrames.pop_front();
                delete mltFrame;
                // Seek
                if ( toSeek )
                {
                    mlt_log_error( vlcProducer->m_parent->get_service(), "Sought\n" );
                    vlcProducer->m_mediaPlayer->setTime( vlcProducer->m_lastPosition * 1000.0 / mlt_profile_fps( ( mlt_profile ) vlcProducer->m_parent->get_data( "_profile" ) ) + 0.5 );
                    vlcProducer->clearFrames();
                }
            }
        }
        
        vlcProducer->renderLock.unlock();
        return 0;
    }
    
    void clearFrames()
    {
        for ( auto* frame : m_audioFrames )
            delete frame;
        m_audioFrames.clear();
        
        for ( auto* frame : m_videoFrames )
            delete frame;
        m_videoFrames.clear();
        
        for ( auto* frame : m_mltFrames )
            delete frame;
        m_mltFrames.clear();
    }
    
    Mlt::Producer*      m_parent;
    
    VLC::Instance*      m_instance;
    VLC::Media*         m_media;
    VLC::MediaPlayer*   m_mediaPlayer;
    
    std::deque<Frame*>  m_videoFrames;
    std::deque<Frame*>  m_audioFrames;
    
    std::deque<Mlt::Frame*> m_mltFrames;
    
    int                 m_audioIndex;
    int                 m_videoIndex;
    int                 m_lastPosition;
    
    std::mutex          renderLock;
};

extern "C" mlt_producer producer_vlc_init_CXX( mlt_profile profile, mlt_service_type type , const char* id , char* arg )
{
    return ( new VLCProducer( profile, arg ) )->producer();
}
