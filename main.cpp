#include <stdio.h>
#include <unistd.h>     // UNIX standard function definitions
#include <fcntl.h>      // File control definitions
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions

#include "ThinkGearStreamParser.h"
#define OSCPKT_OSTREAM_OUTPUT
#include "oscpkt.hh"
#include "udp.hh"
#include "Audio.h"

#include "AppParams.h"

void handleSignal( float v );
void handleAttention( float v );
void handleMeditation( float v );
void handleEeg( int* vals );
void handleRaw( float v );
#include "RecorderPlayback.h"

using namespace oscpkt;

PacketReader pr;

UdpSocket sock;
std::string root = "/mindwave/1";
UdpSocket raw_sock;

AppParams params;


void sendOscStatus( std::string status )
{
    PacketWriter pw;
    Message msg(root+"/"+status);
    pw.startBundle().addMessage(msg).endBundle();
    if(!sock.sendPacket(pw.packetData(), pw.packetSize()))
        printf( "Could not send status: %s\n", status.c_str() );
}

void handleSignal( float v )
{
    printf( "Signal Level: %f\n", v );
    RecorderPlayback::signal(v);

    PacketWriter pw;
    Message msg(root+"/signal");
    msg.pushFloat(v);
    pw.startBundle().addMessage(msg).endBundle();
    if(!sock.sendPacket(pw.packetData(), pw.packetSize()))
        printf( "Could not send sign packet\n" );
}

void handleAttention( float v )
{
    RecorderPlayback::attention(v);
    printf( "Attention Level: %f\n", v);
    PacketWriter pw;
    Message msg(root+"/attention");
    msg.pushFloat(v);
    pw.startBundle().addMessage(msg).endBundle();
    if(!sock.sendPacket(pw.packetData(), pw.packetSize()))
        printf( "Could not send att packet\n" );
}

void handleMeditation( float v )
{
    RecorderPlayback::meditation(v);

    printf( "Meditation Level: %f\n", v );
    PacketWriter pw;
    Message msg(root+"/meditation");
    msg.pushFloat(v);
    pw.startBundle().addMessage(msg).endBundle();
    if(!sock.sendPacket(pw.packetData(), pw.packetSize()))
        printf( "Could not send med packet\n" );
}

void handleEeg( int* vals )
{
    RecorderPlayback::eeg(vals);

    PacketWriter pw;
    Message msg(root+"/eeg");

    for(int i = 0; i < 8; ++i) {
        msg.pushInt32(vals[i]);
    }

    pw.startBundle().addMessage(msg).endBundle();

    if(!sock.sendPacket(pw.packetData(), pw.packetSize()))
        printf( "Could not send eeg packet\n" );
}

void handleRaw( float v )
{
#ifdef AUDIO_NULL
    RecorderPlayback::raw(v);

    if(!params.audio->isStreamRunning())
        params.audio->startStream();

    // Add a slow ramp up when first connected
    static int count = 0;
    float gain = 1.0f;
    if(count < 0)
    {
        gain = 0.0f;
        ++count;
    }
    else if(count < 4096)
    {
        gain = (count-4096)/4096.0f;
        ++count;
    }

    params.audio->addRaw(gain*params.gain*v);
#endif
}

bool sendConnect = false;
bool isConnected = false;
void
handleDataValueFunc( unsigned char extendedCodeLevel,
                     unsigned char code,
                     unsigned char valueLength,
                     const unsigned char *value,
                     void *customData ) 
{
	Audio* audio = (Audio*)customData;
    switch( code ) {

        case( 0x02 ): {
            isConnected = true;
            handleSignal(1.0f-((value[0] & 0xFF)/200.0f));
            } break;

        case( 0x04 ): {
            isConnected = true;
            handleAttention((value[0] & 0xFF)/100.0);
            } break;

        case( 0x05 ): {
            isConnected = true;
            handleMeditation((value[0] & 0xFF)/100.0);
            } break;

        case( 0x83 ): {
            isConnected = true;
            int vals[8];
            for(int i = 0; i < 8; ++i) {
                vals[i] = ((value[i+2]&0xFF)<<16)+((value[i+1]&0xFF)<<8)+((value[i+0]&0xFF));
            }
            handleEeg(vals);
            } break;

        case( 0xD4 ):
            printf( "Standby/Scan\n" );
            sendConnect = true;
            sendOscStatus("scanning");

            //isConnected = false;
            break;

        case( 0xD0 ):
            printf( "Connected!\n" );
            break;

        case( 0xD2 ):
            printf( "Disconnected!\n" );
            sendConnect = true;
            //isConnected = false;
            break;

        case( 0xD3 ):
            printf( "Denied!\n" );
            sendConnect = true;
            sendOscStatus("denied");
            //isConnected = false;
            break;

        case( 0x80 ): {
            int raw = (value[1]&0xFF)+((value[0]&0xFF)<<8);
            if(raw >= 32768)
                raw-=65536;

            // Sometimes there is still some raw data "in the pipe"
            // So ignore any until it connects
            if(isConnected) {
                handleRaw(float(raw)/32768.0f);
            }
		 } break;

        /* Other [CODE]s */
        default:
            printf( "EXCODE level: %d CODE: 0x%02X vLength: %d\n",
                    extendedCodeLevel, code, valueLength );
            printf( "Data value(s):" );
            for( int i=0; i<valueLength; i++ ) printf( " %02X", value[i] & 0xFF );
            printf( "\n" );
            //exit(1);
			break;
    }
}

//typedef FILE* SERIAL;
typedef int SERIAL;
SERIAL open_device(std::string dev)
{
    //return fopen( dev.c_str(), "r+" );

    int USB = open( dev.c_str(), O_RDWR| O_NOCTTY | O_NDELAY );

    /* Error Handling */
    if ( USB < 0 )
    {
        std::cout << "Error " << errno << " opening " << "/dev/ttyUSB0" << ": " << strerror (errno) << std::endl;
    }

    /* *** Configure Port *** */
    struct termios tty;
    memset (&tty, 0, sizeof tty);

    /* Error Handling */
    if ( tcgetattr ( USB, &tty ) != 0 )
    {
        std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
    }

    /* Set Baud Rate */
    cfsetospeed (&tty, B115200);
    cfsetispeed (&tty, B115200);

    /* Setting other Port Stuff */
    tty.c_cflag     &=  ~PARENB;        // Make 8n1
    tty.c_cflag     &=  ~CSTOPB;
    tty.c_cflag     &=  ~CSIZE;
    tty.c_cflag     |=  CS8;
    tty.c_cflag     &=  ~CRTSCTS;       // no flow control
    tty.c_lflag     =   0;          // no signaling chars, no echo, no canonical processing
    tty.c_oflag     =   0;                  // no remapping, no delays
    tty.c_cc[VMIN]      =   0;                  // read doesn't block
    tty.c_cc[VTIME]     =   0;                  // 0.5 seconds read timeout

    tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines
    tty.c_iflag     &=  ~(IXON | IXOFF | IXANY);// turn off s/w flow ctrl
    tty.c_lflag     &=  ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    tty.c_oflag     &=  ~OPOST;              // make raw

    /* Flush Port, then applies attributes */
    tcflush( USB, TCIFLUSH );

    if ( tcsetattr ( USB, TCSANOW, &tty ) != 0)
    {
        std::cout << "Error " << errno << " from tcsetattr" << std::endl;
    }

    return USB;
}



unsigned char dread(SERIAL dev)
{
    unsigned char byte;
    //fread( &byte, 1, 1, dev );
    //while(read(dev, &byte, 1)==0) {}
    while(true) {
        fd_set readfs;
        FD_ZERO(&readfs);
        FD_SET(dev, &readfs);
        select(dev+1, &readfs, NULL, NULL, NULL);
        if( FD_ISSET(dev, &readfs) )
        {
            if(read(dev, &byte, 1) != 0)
                return byte;
        }
    }
}

void dwrite(SERIAL dev, unsigned char byte)
{
    //fwrite( &byte, 1, 1, dev);
    write(dev, &byte, 1);
}


/* Usage: brain -d <MindWave device> -s <OSC server to send messages> -a <audio device #>
 *      other:
 *           --probe : List out audio devices and quit
 *           --record <file> record incoming to <file> simultaneously
 *           --playback <file> : Do not connect to MindWave device, instead play from file
 *           --test <num> : Play through test output
 *
 * Tests:
 *   0: Send out sawtooth wave on audio device
 *   1: Replace raw signal output with 1, -1 signal repeating
 */

int main( int argc, char **argv ) {
    if(!params.init(argc, argv)) return 0;

#ifdef AUDIO_NULL
	params.audio = new Audio_Null();
#endif

#ifdef AUDIO_RT
 	params.audio = new Audio_RT();
#endif

#ifdef AUDIO_PA
	params.audio = new Audio_PA();
#endif

    Audio& audio = *params.audio;

	if( params.probe )
	{
		audio.probe();
		return 0;
	}

#ifndef AUDIO_NULL
	if( params.test == 0 )
    {
        audio.startStream();

        char input;
        std::cout << "\nPlaying ... press <enter> to quit.\n";
        std::cin.get( input );

        delete params.audio;
        return 0;
    }
	// How much to preseed?
	for(int i = 0; i < 16; ++i)
		audio.addRaw( 0.0f );//i%2 == 0 ? 1.0f : -1.0f );
#endif

    printf("Config: %s -> %s:%d\n", params.deviceName.c_str(), params.server.c_str(), params.port);

    ThinkGearStreamParser parser;
    THINKGEAR_initParser( &parser, PARSER_TYPE_PACKETS,
                          handleDataValueFunc, (void*)&audio );

    if(params.recording) {
        RecorderPlayback::file = params.file;
        RecorderPlayback::startRecording();
    }

    sock.connectTo(params.server, params.port);

    if(sock.isOk())
        printf("Sending OSC to %s:%d\n", params.server.c_str(), params.port);
    else {
        printf("Failed to connect to %s:%d\n", params.server.c_str(), params.port);
        return 0;
    }

    // Send boot message
    sendOscStatus("boot");

    SERIAL stream ;
    if(!params.playback)
    {
        stream = open_device(params.deviceName);

        if(stream)
        {
            printf("Listening on %s...\n", params.deviceName.c_str());
        }
        else
        {
            printf("Failed to open %s\n", params.deviceName.c_str());
            return 0;
        }
    }

    // Send open message
    sendOscStatus("open");

    if( !params.playback )
    {
    	unsigned char autoC = 0xC2;
    	dwrite( stream, autoC );

        unsigned char streamByte;
        while( 1 ) {
            streamByte = dread( stream );
            THINKGEAR_parseByte( &parser, streamByte );
            if(sendConnect)
                dwrite( stream, autoC );
            sendConnect = false;

        }
    } else {
        RecorderPlayback::file = params.file;
        RecorderPlayback::startPlayback();
    }
}
