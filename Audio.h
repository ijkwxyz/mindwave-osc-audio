#include <stdio.h>

#ifndef AUDIO_NULL
#include "spsc_queue.h"
#include <samplerate.h>
#endif

#ifdef AUDIO_RT
#include "rtaudio.h"
#endif

#ifdef AUDIO_PA
#include "portaudio.h"
#endif

struct Audio;
#define TEMP_SIZE 4096

struct Audio {
    Audio() {
#ifndef AUDIO_NULL
        state = src_callback_new(Audio::src_callback, SRC_SINC_BEST_QUALITY, 1, &src_error_num, this);
#endif
	};
	virtual ~Audio() {}

	virtual void addRaw (float raw) {
#ifndef AUDIO_NULL
		samples.enqueue(raw);
#endif
	}

#ifndef AUDIO_NULL
	float temp2[TEMP_SIZE];
    float temp[512];

	virtual void process_output(float *buffer, int frames)
	{
        float* buff = buffer;

        if(params.test == 1)
        {
            static float last0 = 0.0f;
            static float last1 = 0.0f;
            // Write interleaved audio data.
            for ( int i=0; i<frames; i++ ) {
                *buff++ = last0;
                *buff++ = last1;

                last0 += 0.005f;
                last1 += 0.005f;


                if ( last0 >= 1.0f ) last0 -= 2.0f;
                if ( last1 >= 1.0f ) last1 -= 2.0f;
            }
            return; 
        }

		long left = frames;

		while(true) {
			long read = src_callback_read(state, 44100.0/512.0, std::min(long(TEMP_SIZE), left), temp2);
            if( read <= 0 )
            {
                int err = src_error(state);
                if(err)
                {
                    printf("Error: %s\n", src_strerror(err));
                }
                else
                {
                    printf("Read 0???\n");
                }
            }
            else
            {
                //printf("po %d\n", (int)read);
                left -= read;
                for(int i = 0; i < read; ++i)
                    buff[2*i+0] = buff[2*i+1] = temp2[i];

                buff += 2*read;
                if( left <= 0 )
                    return;
            }
		}
	}

    long process_input(float **data)
    {
        *data = temp;
        float sample;
        long i = -1;
        while( i < 512 && samples.dequeue( temp[++i] ) ) {}
        //printf("pi : %d\n", (int)i);
        return i;
    }

    static long  src_callback(void *cb_data, float **data)
    {
        Audio *audio = (Audio*)cb_data;
        return audio->process_input(data);
    }

	int src_error_num;
	SRC_STATE* state;
	spsc_queue<float> samples;
#endif

	virtual bool isStreamRunning() = 0;
    virtual void startStream() = 0;
    virtual void probe() = 0;
};

#ifdef AUDIO_NULL
struct Audio_Null : Audio {
    Audio_Null() : Audio() {}
    virtual ~Audio_Null() {}
	virtual bool isStreamRunning() { return false; };
	virtual void startStream() {};
	virtual void probe() {};
};
#endif 

#ifdef AUDIO_PA
struct Audio_PA : Audio {
    Audio_PA() : Audio()
    {
        err = Pa_Initialize();
        // print out error txt
        PaStreamParameters output;
        bzero( &output, sizeof(output) );

        output.channelCount = 2;
        output.device = params.audioDevice;
        output.sampleFormat = paFloat32;

        err = Pa_OpenStream(&stream, NULL, &output, 44100.0, paFramesPerBufferUnspecified  /* 512 */, paNoFlag, Audio_PA::pa_callback, (void*)this );
		if( err != paNoError )
			std::cout << "Could not open Stream!" << std::endl;
    }
    virtual ~Audio_PA()
    {
        Pa_Terminate();
    }

    PaError err;
    PaStream* stream;

    virtual bool isStreamRunning()
    {
        return Pa_IsStreamActive(stream) == 1;//device->isStreamRunning();
    }

    virtual void startStream()
    {
        err = Pa_StartStream( stream );
 		if( err != paNoError )
			std::cout << "Could not start Stream!" << std::endl;
	}


    static int pa_callback( const void *, void *outputBuffer,
                     unsigned long framesPerBuffer,
                     const PaStreamCallbackTimeInfo* timeInfo,
                     PaStreamCallbackFlags statusFlags,
                     void *userData ) 
    {
        Audio *audio = (Audio*)userData;
        if(statusFlags != 0 ) //& ppOutputUnderflow)
            printf("Underflow\n"); // Underflow!

        audio->process_output((float*)outputBuffer, framesPerBuffer);
        return 0;
    }

	virtual void probe()
	{
		int numDevices = Pa_GetDeviceCount();
		const PaDeviceInfo *info;
		for( int i=0; i<numDevices; i++ )
		{
			info = Pa_GetDeviceInfo( i );
            std::cout << "device = (" << i << ") : " << info->name;
            std::cout << " : maximum output channels = " << info->maxOutputChannels << "\n";
		}
	}
};
#endif

#ifdef AUDIO_RT

int rt_callback(void *output, void *, unsigned int nFrames, double, RtAudioStreamStatus, void *userData);
void rt_error(RtAudioError::Type type, const std::string &errorText)
{
    printf("Error: %s\n", errorText.c_str());
}

struct Audio_RT : Audio {
	Audio_RT() : Audio()
	{
		unsigned int bufferSize = 512; 
		try {
			device = new RtAudio();

			param.deviceId = params.audioDevice;
			param.nChannels = 2;
			param.firstChannel = 0;
			device->showWarnings(true);
			device->openStream(&param, NULL,  RTAUDIO_FLOAT32, 44100, &bufferSize, Audio_RT::rt_callback, this, NULL, rt_error);
            std::cout << bufferSize << std::endl;
		} 
		catch (RtAudioError &error) {
			error.printMessage();
		}
	}

	virtual ~Audio_RT()
	{
		src_delete(state);
		if(device) {
			device->stopStream();
			device->closeStream();
			delete device;
		}
	}

    virtual bool isStreamRunning()
    {
        return device->isStreamRunning();
    }

    virtual void startStream()
    {
        device->startStream();
    }

    static int rt_callback(void *output, void *, unsigned int nFrames, double, RtAudioStreamStatus status, void *userData)
    {
        Audio *audio = (Audio*)userData;
        if(status != 0)
            printf("Underflow\n"); // Underflow!

        audio->process_output((float*)output, nFrames);
        return 0;
    }

	RtAudio::StreamParameters param;
	RtAudio *device;

	virtual void probe()
	{
        RtAudio device;
        {
            unsigned int devices = device.getDeviceCount();
            RtAudio::DeviceInfo info;
            for ( unsigned int i=0; i<devices; i++ ) {
                info = device.getDeviceInfo( i );
                if ( info.probed == true ) {
                    std::cout << "device = (" << i << ") : " << info.name;
                    std::cout << " : maximum output channels = " << info.outputChannels << "\n";
                }
            }
        }
	}
};
#endif
