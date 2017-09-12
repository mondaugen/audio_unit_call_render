#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <signal.h>
#include <unistd.h>
#define STRBUFSIZE 1000
#define AUDIO_BUF_LEN 512
#define AUDIO_BUF_PRINT_LEN 10
static char strbuf[STRBUFSIZE];

#define CHK_ERR(expr, ...)                                                     \
    do {                                                                       \
        if ((expr)) {                                                          \
            fprintf(stderr, "%s:%d ", __func__, __LINE__);                     \
            fprintf(stderr, "error: " __VA_ARGS__);                            \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

static volatile int done = 0;

static AudioUnit au, au_output;

void
setdone(int sn)
{
    done = 1;
}

void
afr_completion_proc(void *aux, ScheduledAudioFileRegion *afr, OSStatus result)
{
    fprintf(stderr, "Region completed, status %d\n", result);
    //    done = 1;
}

static OSStatus
renderCallback(void *                      inRefCon,
               AudioUnitRenderActionFlags *ioActionFlags,
               const AudioTimeStamp *      inTimeStamp,
               UInt32                      inBusNumber,
               UInt32                      inNumberFrames,
               AudioBufferList *           ioData)
{
    OSStatus oss;
    /* Proof that HostTime is not necessary for AudioFilePlayer */
    AudioTimeStamp myts = *inTimeStamp;
    myts.mFlags = kAudioTimeStampSampleTimeValid;
    myts.mHostTime = 0;
    CHK_ERR(oss = AudioUnitRender(
              au, ioActionFlags, &myts, 0, inNumberFrames, ioData),
            "%d rendering",
            oss);
    fprintf(stderr, "ioActionFlags = %#x\n", (unsigned int)(*ioActionFlags));
    fprintf(stderr, "inNumberFrames = %d\n", (int)inNumberFrames);
    fprintf(stderr, "inTimeStamp:\n");
    if (inTimeStamp->mFlags & kAudioTimeStampSampleTimeValid) {
        fprintf(stderr, "   sampleTime: %f\n", inTimeStamp->mSampleTime);
    }
    if (inTimeStamp->mFlags & kAudioTimeStampHostTimeValid) {
        fprintf(stderr, "   HostTime: %llu\n", inTimeStamp->mHostTime);
    }
    fprintf(stderr, "bufferList:\n");
    fprintf(stderr, "    numberBuffers: %d\n", ioData->mNumberBuffers);
    size_t m;
    for (m = 0; m < ioData->mNumberBuffers; m++) {
        fprintf(stderr, "    buffer[%zu]:\n", m);
        fprintf(stderr,
                "        NumberChannels: %d\n",
                ioData->mBuffers[m].mNumberChannels);
        fprintf(stderr,
                "        DataByteSize: %d\n",
                ioData->mBuffers[m].mDataByteSize);
        fprintf(stderr, "        Data: %#x\n", (int)ioData->mBuffers[m].mData);
    }
    //    ioData->mNumberBuffers=2;

    return oss;
}

static void
make_AudioFilePlayer(AudioUnit *_au)
{
    AudioComponentDescription ac_desc = {
        .componentType = kAudioUnitType_Generator,
        .componentSubType = kAudioUnitSubType_AudioFilePlayer,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };
    UInt32         nComponents = AudioComponentCount(&ac_desc);
    AudioComponent ac = NULL;
    CFStringRef    ac_name;
    printf("number of components: %u\n", nComponents);
    while (nComponents--) {
        ac = AudioComponentFindNext(ac, &ac_desc);
        AudioComponentCopyName(ac, &ac_name);
        if (CFStringGetCString(
              ac_name, strbuf, STRBUFSIZE, kCFStringEncodingUTF8)) {
            printf("name: %s\n", strbuf);
        }
    }
    OSStatus oss;
    CHK_ERR(
      oss = AudioComponentInstanceNew(ac, _au), "%d getting instance", oss);
    CHK_ERR(oss = AudioUnitInitialize(*_au), "%d intializing audio unit", oss);
}

static void
make_DefaultOutput(AudioUnit *_au)
{
    AudioComponentDescription ac_desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
        .componentFlags = 0,
        .componentFlagsMask = 0
    };
    UInt32         nComponents = AudioComponentCount(&ac_desc);
    AudioComponent ac = NULL;
    CFStringRef    ac_name;
    printf("number of components: %u\n", nComponents);
    while (nComponents--) {
        ac = AudioComponentFindNext(ac, &ac_desc);
        AudioComponentCopyName(ac, &ac_name);
        if (CFStringGetCString(
              ac_name, strbuf, STRBUFSIZE, kCFStringEncodingUTF8)) {
            printf("name: %s\n", strbuf);
        }
    }
    OSStatus oss;
    CHK_ERR(
      oss = AudioComponentInstanceNew(ac, _au), "%d getting instance", oss);
    CHK_ERR(oss = AudioUnitInitialize(*_au), "%d intializing audio unit", oss);
    AURenderCallbackStruct renderCallbackStruct;
    renderCallbackStruct.inputProc = renderCallback;
    renderCallbackStruct.inputProcRefCon = NULL;
    CHK_ERR(oss = AudioUnitSetProperty(*_au,
                                       kAudioUnitProperty_SetRenderCallback,
                                       kAudioUnitScope_Input,
                                       0,
                                       &renderCallbackStruct,
                                       sizeof renderCallbackStruct),
            "%d setting output callback",
            oss);
}

int
main(void)
{
    int realtime = 0;
    make_AudioFilePlayer(&au);
    OSStatus oss;
    /* TODO: Make Mono output.
       So far I haven't been able to set this to anything other than the format
       it returns.
       To get the format, you have to set size of return type on call (e.g.,
       asbd_sz), and it will contain the size of the returned data after the
       call. */
    // AudioStreamBasicDescription asbd = {
    //    .mSampleRate = 44100,
    //    .mFormatID = 'lpcm',
    //    .mFormatFlags = 0x29,
    //    .mBytesPerPacket = 9,
    //    .mFramesPerPacket = 1,
    //    .mBytesPerFrame = 8,
    //    .mChannelsPerFrame = 2,
    //    .mBitsPerChannel = 32,
    //    .mReserved = 0
    //};
    AudioStreamBasicDescription asbd;
    UInt32                      asbd_sz = sizeof(AudioStreamBasicDescription);
    CHK_ERR(oss = AudioUnitGetProperty(au,
                                       kAudioUnitProperty_StreamFormat,
                                       kAudioUnitScope_Output,
                                       0,
                                       &asbd,
                                       &asbd_sz),
            "%d getting stream format",
            oss);
    // asbd.mChannelsPerFrame = 1;
    // asbd.mBytesPerFrame = asbd.mBitsPerChannel / 8 * asbd.mChannelsPerFrame;
    CHK_ERR(oss = AudioUnitSetProperty(au,
                                       kAudioUnitProperty_StreamFormat,
                                       kAudioUnitScope_Output,
                                       0,
                                       &asbd,
                                       sizeof(AudioStreamBasicDescription)),
            "%d setting stream format",
            oss);
    CFURLRef af_url =
      CFURLCreateWithString(NULL, CFSTR("/tmp/example.aif"), NULL);
    AudioFileID af_id;
    AudioFileOpenURL(af_url, kAudioFileReadPermission, 0, &af_id);
    CHK_ERR(oss = AudioUnitSetProperty(au,
                                       kAudioUnitProperty_ScheduledFileIDs,
                                       kAudioUnitScope_Global,
                                       0,
                                       &af_id,
                                       sizeof(AudioFileID)),
            "%d",
            oss);
    /* As it says in the docs, this needs to be called before "starting".
       Starting seems to mean setting kAudioUnitProperty_ScheduleStartTimeStamp.
     */
    AudioTimeStamp ats = {
        .mSampleTime = 0, .mFlags = kAudioTimeStampSampleTimeValid,
    };
    ScheduledAudioFileRegion sched_afr = {
        .mTimeStamp = ats,
        .mCompletionProc = afr_completion_proc,
        .mCompletionProcUserData = NULL,
        .mAudioFile = af_id,
        .mLoopCount = 0,
        .mStartFrame = 0,
        .mFramesToPlay = 100000,
    };
    CHK_ERR(oss = AudioUnitSetProperty(au,
                                       kAudioUnitProperty_ScheduledFileRegion,
                                       kAudioUnitScope_Output,
                                       0,
                                       &sched_afr,
                                       sizeof(ScheduledAudioFileRegion)),
            "%d scheduling audio file region",
            oss);
    UInt32 nprime_frames = 0;
    CHK_ERR(oss = AudioUnitSetProperty(au,
                                       kAudioUnitProperty_ScheduledFilePrime,
                                       kAudioUnitScope_Output,
                                       0,
                                       &nprime_frames,
                                       sizeof(nprime_frames)),
            "%d priming audio regions",
            oss);
    /* This seems to start playback, too. */
    CHK_ERR(oss =
              AudioUnitSetProperty(au,
                                   kAudioUnitProperty_ScheduleStartTimeStamp,
                                   kAudioUnitScope_Global,
                                   0,
                                   &ats,
                                   sizeof(ats)),
            "%d setting start time stamp",
            oss);
    signal(SIGINT, setdone);
    if (realtime) {
        make_DefaultOutput(&au_output);
        AudioOutputUnitStart(au_output);
        while (!done)
            ;
    } else {
        char abl_dat[sizeof(AudioBufferList) + 2 * sizeof(AudioBuffer)];

        AudioBufferList *abl = (AudioBufferList *)abl_dat;
        abl->mNumberBuffers = 2;
        float abuffer[2 * AUDIO_BUF_LEN];
        abl->mBuffers[0] = (AudioBuffer){
            .mNumberChannels = 1,
            .mDataByteSize = sizeof(float) * AUDIO_BUF_LEN,
            .mData = abuffer,
        };
        abl->mBuffers[1] = (AudioBuffer){
            .mNumberChannels = 1,
            .mDataByteSize = sizeof(float) * AUDIO_BUF_LEN,
            .mData = abuffer + AUDIO_BUF_LEN,
        };
        AudioUnitRenderActionFlags ra_flags = 0;
        AudioTimeStamp             ats_rt = { .mSampleTime = 0,
                                  .mFlags = kAudioTimeStampSampleTimeValid };

        while (!done) {
            CHK_ERR(oss = AudioUnitRender(
                      au, &ra_flags, &ats_rt, 0, AUDIO_BUF_LEN, abl),
                    "%d rendering",
                    oss);
            size_t n;
            for (n = 0; n < AUDIO_BUF_PRINT_LEN; n++) {
                printf("%f\n", ((float *)abl->mBuffers[0].mData)[n]);
            }
            printf("\n");
            for (n = 0; n < AUDIO_BUF_PRINT_LEN; n++) {
                printf("%f\n", ((float *)abl->mBuffers[1].mData)[n]);
            }
            printf("\n");
            ats_rt.mSampleTime += AUDIO_BUF_LEN;
            sleep(1);
        }
    }
    AudioOutputUnitStop(au_output);
    AudioFileClose(af_id);
    AudioComponentInstanceDispose(au);
    return 0;
}
