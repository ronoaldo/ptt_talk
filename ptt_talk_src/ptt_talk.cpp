/** @ptt talk.cpp
    @brief Record input into an ogg file; Playback recorded data.
    @author APercy - adapted code from Phil Burk  http://www.softsynth.com
*/
/*
 * $Id$
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however,
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also
 * requested that these non-binding requests be included along with the
 * license above.
 */


#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h> /* For O_* constants */
#include <strings.h>
#include <string.h>

#include <sys/stat.h>
#include <fstream>

#include<X11/X.h>
#include<X11/Xlib.h>
#include<X11/Xutil.h>
#include <unistd.h>
#include <thread>

#include "portaudio.h"
#include "ptt_talk.h"
#include "audio_rec_play.h"

char *hostname;
char *nick;

/*******************************************************************/
int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <nick>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    nick = argv[2];

    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;
    bool                should_stop = false;
    bool                is_recording = false;

    printf("ptt_talk.cpp\n"); fflush(stdout);
 
    /* ------------------- set key event ------------------- */
    Display* display = XOpenDisplay(NULL);
    if(display == NULL) {
        fprintf(stderr,"Cannot open display.\n");
        return done(&data, err);
    }
    int screen = DefaultScreen(display);
    Window root = DefaultRootWindow(display);
    /* create window */
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 10, 10, 200, 200, 1,
                           BlackPixel(display, screen), WhitePixel(display, screen));

    Window curFocus = window;
    char buf[17];
    KeySym ks;
    XComposeStatus comp;
    int len;
    int revert;

    unsigned int keycode = XKeysymToKeycode(display, XK_Tab);
    XGetInputFocus (display, &curFocus, &revert);
    XSelectInput(display, window, KeyPressMask|KeyReleaseMask|FocusChangeMask);
    /* map (show) the window */
    XMapWindow(display, window);

    XEvent ev;
    /* ------------------- end key event ------------------- */

    float last_time = 0.0;
    float sleep_time = 0.3;
    while (1)
    {
        sleep(sleep_time);
        last_time += sleep_time;
        if(last_time > 0.5) last_time = 0.5;
        XNextEvent(display, &ev);
        switch (ev.type) {
            case FocusOut:
                /*printf ("Process changed!\n");
                printf ("Old Process is %d\n", (int)curFocus);*/
                if (curFocus != root)
                    XSelectInput(display, curFocus, 0);
                XGetInputFocus (display, &curFocus, &revert);
                //printf ("New Process is %d\n", (int)curFocus);
                if (curFocus == PointerRoot)
                    curFocus = root;
                XSelectInput(display, curFocus, KeyPressMask|KeyReleaseMask|FocusChangeMask);
                break;
            case KeyRelease:
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                /*printf ("Released key!\n");
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                if (len > 0 && isprint(buf[0]))
                {
                    buf[len]=0;
                    printf("String is: %s\n", buf);
                }
                else
                {
                    printf ("Key is: %d\n", (int)ks);
                }*/
                break;
            case KeyPress:
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                /*printf ("Got key!\n");
                len = XLookupString(&ev.xkey, buf, 16, &ks, &comp);
                if (len > 0 && isprint(buf[0]))
                {
                    buf[len]=0;
                    printf("String is: %s\n", buf);
                }
                else
                {
                    printf ("Key is: %d\n", (int)ks);
                }*/

                /* Record some audio. -------------------------------------------- */
                if( ks == 65289 ) { //--- tab key
                    printf ("Tab pressed!\n");
                    if( is_recording == false ) { //isn't recording
                        if(last_time >= 0.5) {
                            last_time = 0.0;
                            printf(">>> starting\n");
                            is_recording = true; //so start the record
                            should_stop = false;
                            initialize(&inputParameters, stream, err, &data, &numSamples, &numBytes, &totalFrames, &should_stop);
                            //recordAudio(inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                            std::thread thread(recordAudio, inputParameters, stream, err, data, numSamples, &should_stop, &is_recording);
                            thread.detach();
                        }
                    } else {
                        if (should_stop == false && is_recording == true) {
                            if(last_time >= 0.5) {
                                last_time = 0.0;
                                printf(">>> stopping\n");
                                should_stop = true;
                            }
                        }
                    }
                }

                /* Playback recorded data.  -------------------------------------------- */
                //playbackAudio( outputParameters, stream, err, data);
                break;

        } //end switch

    } //end while
}

int sendMessage(char* file_path) {
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];

    portno = 41000;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        printf("ERROR opening socket");
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    //printf("socket openned\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
         printf("ERROR connecting");

    bzero(buffer,256);
    strncpy(buffer, nick, sizeof(buffer));
    strncat(buffer, "\n", sizeof(buffer));
    //n = send(sockfd , buffer , strlen(nick) , 0 );
    n = write(sockfd,buffer,strlen(buffer));
    //printf ("return write %d\n", (int)n);
    if (n < 0) 
         printf("ERROR writing to socket");
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         printf("ERROR reading from socket");
    //printf("Socket return %s\n",buffer);
    std::string str_buffer = (std::string) buffer;
    if(str_buffer == (std::string) "file\n") {
        //TODO mandar arquivo
        int64_t rc = SendFile(sockfd, "recorded.ogg");
        if (rc < 0) {
            printf("Failed to send file: %ld\n", rc);
        }
    }
    close(sockfd);
    return 0;
}

///
/// Sends data in buffer until bufferSize value is met
///
int SendBuffer(int socket, const char* buffer, int bufferSize, int chunkSize) {

    int i = 0;
    while (i < bufferSize) {
        const int l = send(socket, &buffer[i], std::min(chunkSize, bufferSize - i), 0);
        if (l < 0) { return l; } // this is an error
        i += l;
    }
    return i;
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

//
// Sends a file
// returns size of file if success
// returns -1 if file couldn't be opened for input
// returns -2 if couldn't send file length properly
// returns -3 if file couldn't be sent properly
//
int64_t SendFile(int socket, const std::string& fileName, int chunkSize) {

    const int64_t fileSize = GetFileSize(fileName);
    //printf("File size: %ld\n", fileSize);
    if (fileSize < 0) { return -1; }

    std::ifstream file(fileName, std::ifstream::binary);
    if (file.fail()) { return -1; }
    //printf("Stream created\n");

    char* buffer = new char[chunkSize];
    bool errored = false;
    int64_t i = fileSize;
    while (i != 0) {
        const int64_t ssize = std::min(i, (int64_t)chunkSize);
        if (!file.read(buffer, ssize)) {
            errored = true;
            break;
        }
        const int l = SendBuffer(socket, buffer, (int)ssize);
        //printf("buffer__%s\n", buffer);
        if (l < 0) {
            errored = true;
            break;
        }
        i -= l;
    }
    delete[] buffer;

    file.close();

    return errored ? -3 : fileSize;
}
