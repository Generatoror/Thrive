// Copyright (C) 2013-2017  Revolutionary Games

#pragma once

#include "GameFramework/Actor.h"

#include "PlayerSoundWaveParent.h"

#if WITH_FFMPEG != 1
#error Trying to compile without ffmpeg enabled in the Thrive.Build.cs for this platform
#endif

#if WITH_PORTAUDIO != 1
#error Trying to compile without portaudio enabled in the Thrive.Build.cs for this platform
#endif


extern "C"{
// FFMPEG includes
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "portaudio.h"

#include <chrono>
#include <mutex>
#include <memory>
#include <list>


#include "ThriveVideoPlayer.generated.h"


class FFileReadHelper{
public:

    FFileReadHelper(const FString &ReadFile);

    ~FFileReadHelper();

    bool IsValid() const;

    int Read(uint8_t* Buffer, int BufferSize);

    //! Doesn't actually do anything as the name implies
    int Write(uint8_t *Buffer, int BufferSize);

    //! Whence is some FFMPEG magic thing
    int64_t Seek(int64_t Offset, int Whence);
private:

    IFileHandle* File = nullptr;
};

//! \note Due to UE4 things we need to actually define all the ffmpeg things in the header
UCLASS()
class THRIVE_API AThriveVideoPlayer : public AActor
{
	GENERATED_BODY()

    //! This has to be on a single line to work
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoPlaybackEndedDelegate, FString, VideoFile);

protected:

    using ClockType = std::chrono::steady_clock;

    enum class EPacketReadResult{

        Ended,
        Ok,
        QueueFull
    };

    enum class EDecodePriority{

        Video,
        Audio
    };

    //! Holds converted audio data that could not be immediately returned by readAudioData
    struct ReadAudioPacket{

        TArray<uint8_t> DecodedData;
    };

    //! Holds raw packets before sending
    struct ReadPacket{

        ReadPacket(AVPacket* src){

            av_packet_move_ref(&packet, src);
        }

        ~ReadPacket(){

            av_packet_unref(&packet);
        }

        AVPacket packet;
    };
    
public:	
	// Sets default values for this actor's properties
	AThriveVideoPlayer();

    ~AThriveVideoPlayer();

    //! Called automatically to update the output texture
    //! \note This doesn't care about DeltaTime so the video cannot be automatically paused
    //! or its speed altered. Maybe if the streaming audio thing also follows DeltaTime
    //! (not sure if that is the case) then this could be also switched to use DeltaTime
    //! instead of an external timer
    //! \todo Allow this to skip frames if FPS is lower than the video fps.
    //! Right now it just plays in slow motion if the FPS tanks too hard
	virtual void Tick(float DeltaTime) override;

    //! Starts playing a video the playback
    //! \pre The target texture 
    //! \returns False if the file is invalid and playback didn't start
    UFUNCTION(BlueprintCallable, Category="Thrive|VideoPlayer")
    bool PlayVideo(const FString &NewVideoFile);

    //! Closes the video stream
    UFUNCTION(BlueprintCallable, Category="Thrive|VideoPlayer")
    void Close();
    
    //! \returns True if currently loaded file has an audio stream
    bool HasAudio() const;

    //! \returns Current playback position, in seconds
    //! The return value is directly read from the last decoded frame timestamp
    float GetCurrentTime() const;

    //! \returns Width of the current video
    int32_t GetVideoWidth() const;

    //! \returns Height of the current video
    int32_t GetVideoHeight() const;


    //! \returns The number of audio channels
    int GetAudioChannelCount() const;

    //! \returns The number of samples per second of the audio stream
    //! or -1 if no audio streams exist
    int GetAudioSampleRate() const;

    //! \brief Reads audio data to the buffer
    //! \returns The number of bytes read
    //! \param amount The maximum number of bytes to read
    size_t ReadAudioData(uint8_t* Output, size_t Amount);

    //! \todo The PortAudio documentation says that this should only use lightweight
    //! methods like PaUtilRingBuffer to communicate, but right now we will do everything
    //! audio decoding related if the video decoding part hasn't read audio
    int PortAudioDataCallback(const void *input, void *output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags);

    //! \brief Dumps info about loaded ffmpeg streams
    void DumpInfo() const;

    //! \returns true if all the ffmpeg stream objects are valid for playback
    bool IsStreamValid() const;

    //! \brief Tries to call ffmpeg initialization once
    static void LoadFFMPEG();

public:

    UPROPERTY(BlueprintAssignable, Category="Thrive|VideoPlayer")
    FVideoPlaybackEndedDelegate OnPlayBackEnded;
    
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

    //! After loading the video this creates the output texture + material for it
    //! \returns false if the setup fails
    bool OnVideoDataLoaded();


    //! \brief Opens and parses the video info into ffmpeg streams and such
    //! \returns false if something fails
    bool FFMPEGLoadFile();

    //! helper for FFMPEGLoadFile
    //! \returns true on success
    bool OpenStream(unsigned int Index, bool Video);

    //! \brief Decodes one video frame. Returns false if more data is required
    //! by the decoder
    bool DecodeVideoFrame();

    //! \brief Reads a single packet from the stream that matches Priority
    EPacketReadResult ReadOnePacket(EDecodePriority Priority);

    //! \brief Updates the texture
    void UpdateTexture();

    //! \brief Reads already decoded audio data. The audio data TArray must be locked
    //! before calling this
    size_t ReadDataFromAudioQueue(std::lock_guard<std::mutex> &AudioLocked,
        uint8_t* Output, size_t Amount);

    //! \brief Resets timers. Call when playback start or resumes
    void ResetClock();

    //! \brief Called when end of playback has been reached
    //!
    //! Closes the playback and invokes the delegates
    void OnStreamEndReached();

    //! Video streem seaking. Don't use as the audio will get out of sync
    void SeekVideo(float Time);
	
protected:

    UPROPERTY()
    UMaterialInterface* BasePlayerMaterial = nullptr;

    UPROPERTY(BlueprintReadOnly)
    UTexture2D* VideoOutputTexture = nullptr;

    //! Use this to add the video output to some surface
    UPROPERTY(BlueprintReadOnly)
    UMaterialInstanceDynamic* VideoOutput = nullptr;    

    FString VideoFile;

    //! True when playing back something and Tick does things
    bool bIsPlaying = false;

private:

    //! FFMPEG file reader
    std::unique_ptr<FFileReadHelper> VideoFileReader;

    AVIOContext* ResourceReader = nullptr;

    // This seems to be not be used anymore
    //AVCodecContext* Context = nullptr;
    AVFormatContext* FormatContext = nullptr;


    //AVCodecParserContext* VideoParser = nullptr;
    AVCodecContext* VideoCodec = nullptr;
    int VideoIndex = 0;


    //! How many timestamp units are in a second in the video stream
    float VideoTimeBase = 1.f;

    //AVCodecParserContext* AudioParser = nullptr;
    AVCodecContext* AudioCodec = nullptr;
    int AudioIndex = 0;

    AVFrame* DecodedFrame = nullptr;
    AVFrame* DecodedAudio = nullptr;


    //! Once a frame has been loaded to DecodedFrame it is converted
    //! to a format that ue4 texture can accept into this frame
    AVFrame* ConvertedFrame = nullptr;

    uint8_t* ConvertedFrameBuffer = nullptr;

    // Required size for a single converted frame
    size_t ConvertedBufferSize = 0;

    int32_t FrameWidth = 0;
    int32_t FrameHeight = 0;

    SwsContext* ImageConverter = nullptr;

    SwrContext* AudioConverter = nullptr;

    //! Audio sample rate
    int SampleRate = 0; 
    int ChannelCount = 0;

    std::list<std::unique_ptr<ReadAudioPacket>> ReadAudioDataBuffer;
    std::mutex AudioMutex;

    //! Used to start the audio playback once
    bool bIsPlayingAudio = false;

    //! True when PortAudio has been initialized
    bool bIsPortAudioInitialized = false;

    //! Audio output
    PaStream* AudioStream = nullptr;

    // Timing control
    float PassedTimeSeconds = 0.f;
    float CurrentlyDecodedTimeStamp = 0.f;

    bool NextFrameReady = false;

    //! Set to false if an error occurs and playback should stop
    bool bStreamValid = false;

    ClockType::time_point LastUpdateTime;

    std::mutex ReadPacketMutex;
    std::list<std::unique_ptr<ReadPacket>> WaitingVideoPackets;
    std::list<std::unique_ptr<ReadPacket>> WaitingAudioPackets;

};