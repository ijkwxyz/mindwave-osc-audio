#ifndef __APP_PARAMS_H
#define __APP_PARAMS_H

#include <sstream>

struct Audio;

struct AppParams {
    std::string deviceName;
    std::string server;
    int port;
    int audioDevice;
    bool recording;
    bool playback;
    std::string file;
    float gain;
    bool probe;
    int test;

    bool valid;

    Audio* audio;

    void usage(std::string exe) {
        std::cout   << "Usage: " << exe << " -d <MindWave device> -s <OSC server>"
#ifdef AUDIO_NULL
					<< "-a <audio device #>"
#endif
					<< std::endl
                    << "Other:" << std::endl
#ifdef AUDIO_NULL
					<< "   -g <float> : Increase gain of audio wave\n"
                    << "   --probe : List out audio devices and quit\n"
#endif
                    << "   --record <file> record incoming to <file> simultaneously\n"
                    << "   --play <file> : Do not connect to MindWave, instead play from file\n"
                    //<< "   --test <num> : Play test output\n"
                    //<< std::endl
                    //<< "Tests:\n"
                    //<< " *   0: Send out sawtooth wave on audio device " 
                    << std::endl;
    }

    AppParams( ) :
        deviceName("/dev/cu.MindWave"),
        server("localhost"),
        port(9000),
        audioDevice(2),
        recording(false),
        playback(false),
        file(""),
        gain(1.0f),
        probe(false),
        test(-1),
        valid(true)
    {}

    bool init( int argc, char **argv ) {
        int nexti = 0;

    #define FAIL() { usage(argv[0]); valid=false; return false; } 
    #define CNI() { if(i+1 >= argc) FAIL(); } 
    #define PARSE_INTO(str, out) { std::stringstream ss(str); if(!(ss>>out)) FAIL(); }
        for( int i = 1; i < argc; ++i) {
            std::string str(argv[i]);
            if( str == "-d" ) {
                CNI();
                deviceName = argv[++i];
            } else if( str == "-s") {
                CNI();
                server = argv[++i];
                size_t colon = server.find(':');
                if(colon != std::string::npos)
                {
                    std::string portTemp = server.substr(colon+1);
                    server = server.substr(0,colon);
                    PARSE_INTO(portTemp, port);
                }
            } else if( str == "-a") {
                CNI();
                PARSE_INTO(argv[++i], audioDevice);
            } else if( str == "--probe") {
				probe = true;
            } else if( str == "--record") {
                CNI();
                file = argv[++i];
                recording = true;
                playback = false;
            } else if( str == "--play") {
                CNI();
                file = argv[++i];
                recording = false;
                playback = true;
            } else if( str == "--test" ) {
                CNI();
                PARSE_INTO(argv[++i], test);
            } else if( str == "-g" ) {
                CNI();
                PARSE_INTO(argv[++i], gain);
            } else {
                FAIL();
            }
        }
        return valid;
    }

    void printProbe()
    {
    }
};


#endif