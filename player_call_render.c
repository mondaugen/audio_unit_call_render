#include <AudioToolbox/AudioToolbox.h>
#define STRBUFSIZE 1000
static char strbuf[STRBUFSIZE];

#define CHK_ERR(expr, ...)                                                     \
    do {                                                                       \
        if ((expr)) {                                                          \
            fprintf(stderr, "Error: " __VA_ARGS__);                            \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

void
afr_completion_proc(void *aux, ScheduledAudioFileRegion *afr, OSStatus result)
{
    fprintf(stderr, "Region completed, status %d\n", result);
}

int
main(void)
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
    OSStatus  oss;
    AudioUnit au;
    CHK_ERR(
      oss = AudioComponentInstanceNew(ac, &au), "%d getting instance", oss);
    CFURLRef af_url =
      CFURLCreateWithString(NULL, CFSTR("/tmp/example.wav"), NULL);
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
    AudioTimeStamp ats = {
        .mSampleTime = 0, .mFlags = kAudioTimeStampSampleTimeValid,
    };
    ScheduledAudioFileRegion sched_afr = {
        mTimeStamp = ats,
        .mCompletionProc = afr_completion_proc,
        .mCompletionProcUserData = NULL,
        .mAudioFile = af_id,
        .mLoopCount = 1,
        .mStartFrame = 0,
        .mFramesToPlay = 100000,
    };
    CHK_ERR(oss = AudioUnitSetProperty(au,
                                       kAudioUnitProperty_ScheduledFileRegion,
                                       kAudioUnitScope_Global,
                                       0,
                                       &sched_afr,
                                       sizeof(ScheduledAudioFileRegion)),
            "%d",
            oss);
    AudioFileClose(af_id);
    AudioComponentInstanceDispose(au);
    return 0;
}
