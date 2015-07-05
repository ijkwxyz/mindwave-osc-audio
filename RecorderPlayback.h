#ifndef _RECORD_PLAYBACK_H_
#define _RECORD_PLAYBACK_H_

#include <fstream>
#include <chrono>
#include <thread>

struct DataPackage {
    float signal;
    float attention;
    float meditation;
    int eeg[8];
    float raw[512];
};

struct PackageFactory {
    PackageFactory(std::string file)
    : fout(file, std::ios::binary )
    {
        newPackage();
    }

    enum { SIGNAL=0, ATTENTION=1, MEDITATION=2, EEG=3, RAW=4 };

    void signal(float v) {
        if( !fin[SIGNAL] )
        {
            dp.signal = v;
            fin[SIGNAL] = true;
        } else {
            flushPackage();
        }

    }

    void attention(float v) {
        if( !fin[ATTENTION] )
        {
            dp.attention = v;
            fin[ATTENTION] = true;
        } else {
            flushPackage();
        }

    }

    void meditation(float v)  {
        if( !fin[MEDITATION] )
        {
            dp.meditation = v;
            fin[MEDITATION] = true;
        } else {
            flushPackage();
        }

    }

    void eeg(int *v)  {
        if( !fin[EEG] )
        {
            for(int i=0; i <8; ++i )
                dp.eeg[i] = v[i];
            fin[EEG] = true;
        } else {
            flushPackage();
        }
    }

    void raw(float v)  {
        if( raw_received < 512  )
        {
            dp.raw[raw_received++] = v;
        } else {
            flushPackage();
        }

    }

private:
    void newPackage() {
        fin[0] = fin[1] = fin[2] = fin[3] = false;
        raw_received = 0;
        dp.signal = dp.attention = dp.meditation = -1.0f;
        for(int i=0; i <8; ++i )
            dp.eeg[i] = -1;
    }

    bool fin[4];
    int raw_received;

    void flushPackage() {
		/*
        if( !fin[0] || !fin[1] || !fin[2] || !fin[3] || raw_received != 512) 
        {
            printf("Incomplete package! :\n");
            if(!fin[EEG])
            {
                printf("\tEEG\n");
            }      
            if(!fin[ATTENTION])
            {
                printf("\tATTENTION\n");            
            }      
            if(!fin[MEDITATION])
            {
                printf("\tMEDITATION\n");            
            }      
            if(!fin[SIGNAL])
            {
                printf("\tSIGNAL\n"); 
            }      
            if( raw_received != 512 )
            {
                printf("\tRAW: %d\n", raw_received );         
            }      
        } */
        for(int i = raw_received; i < 512; ++i)
            dp.raw[i] = 0.0f;

        fout.write((const char*)&dp, sizeof(dp));

        newPackage();
    }
    DataPackage dp;
    std::ofstream fout;
};

struct RecorderPlayback {
    static void startPlayback() {
        pf = NULL;
        playback = true;
        recording = false;
        DataPackage dp;
        std::ifstream fin(file, std::ios::binary );
        while(fin) {
            std::chrono::milliseconds ms1 = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch()
            );
            fin.read((char*)&dp, sizeof(dp));
            if(dp.signal >= 0.0f)
                handleSignal(dp.signal);
            if(dp.attention >= 0.0f)
                handleAttention(dp.attention);
            if(dp.meditation >= 0.0f)
                handleMeditation(dp.meditation);
            if(dp.eeg[0] >= 0)
                handleEeg(dp.eeg);
            for(int i = 0; i<512; ++i)
                handleRaw(dp.raw[i]);
            std::chrono::milliseconds ms2 = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch()
            )-ms1; 
            std::this_thread::sleep_for(std::chrono::milliseconds(1000)-ms2);
        }
    }
    
    static void startRecording() {
        playback = false;
        recording = true;
        pf = new PackageFactory(file);
    }

    static PackageFactory* pf;
    static void stop() {}

    static void signal(float v) { if(pf) pf->signal(v); }
    static void attention(float v) { if(pf) pf->attention(v); }
    static void meditation(float v) { if(pf) pf->meditation(v); }
    static void eeg(int *v) { if(pf) pf->eeg(v); }
    static void raw(float v) { if(pf) pf->raw(v); }

    static std::string file;
    static bool recording;
    static bool playback;
};
#endif


bool RecorderPlayback::recording = false;
bool RecorderPlayback::playback = false;
PackageFactory* RecorderPlayback::pf = NULL;
std::string RecorderPlayback::file = "";

