// sound.cpp: uses fmod on windows and sdl_mixer on unix (both had problems on the other platform)

#include "cube.h"

#ifndef WIN32
#define USE_MIXER
#endif

VAR(soundvol, 0, 255, 255);
VAR(musicvol, 0, 128, 255);

#ifdef USE_MIXER
    #include "SDL_mixer.h"
    #define MAXVOL MIX_MAX_VOLUME
    Mix_Music *mod = NULL;
#else
    #include "fmod.h"
    #define MAXVOL 255
    FMUSIC_MODULE *mod = NULL;
#endif

void stopsound()
{
    if(mod)
    {
        #ifdef USE_MIXER
            Mix_HaltMusic();
            Mix_FreeMusic(mod);
        #else
            FMUSIC_FreeSong(mod);
        #endif
        mod = NULL;
    };
};

void cleansound()
{
    stopsound();
    #ifdef USE_MIXER
        Mix_CloseAudio();
    #else
        FSOUND_Close();
    #endif
};

void initsound()
{
    #ifdef USE_MIXER
        if(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 512)<0)
        {
            conoutf("sound init failed (SDL_mixer): %s",  Mix_GetError());
            soundvol = 0;
        };
    #else
        if(FSOUND_GetVersion()<FMOD_VERSION) fatal("old FMOD dll");
        if(!FSOUND_Init(22050, 32, FSOUND_INIT_GLOBALFOCUS))
        {
            conoutf("sound init failed (FMOD): %d", FSOUND_GetError());
            soundvol = 0;
        };
    #endif
};

void music(char *name)
{
    if(soundvol && musicvol)
    {
        stopsound();
        string sn;
        strcpy_s(sn, "packages/");
        strcat_s(sn, name);
        #ifdef USE_MIXER
            mod = Mix_LoadMUS(path(sn));
            if(mod)
            {
                Mix_PlayMusic(mod, -1);
                Mix_VolumeMusic((musicvol*MAXVOL)/255);
            }
        #else
            mod = FMUSIC_LoadSong(path(sn));
            if(mod)
            {
                FMUSIC_PlaySong(mod);
                FMUSIC_SetMasterVolume(mod, musicvol);
            };
        #endif
    };
};

COMMAND(music, ARG_1STR);

#ifdef USE_MIXER
typedef Mix_Chunk *sample_t;
#else
typedef FSOUND_SAMPLE *sample_t;
#endif



vector<sample_t> samples;
cvector snames;

int registersound(char *name)
{
    loopv(snames) if(strcmp(snames[i], name)==0) return i;
    snames.add(newstring(name));
    samples.add(NULL);
    return samples.length()-1;
};

COMMAND(registersound, ARG_1EST);

void playsoundc(int n) { addmsg(0, 2, SV_SOUND, n); playsound(n); };

int soundsatonce = 0, lastsoundmillis = 0;

void playsound(int n, vec *loc)
{
    if(!soundvol) return;
    if(lastmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
    lastsoundmillis = lastmillis;
    if(soundsatonce>5) return;  // avoid bursts of sounds with heavy packetloss and in sp
    if(n<0 || n>=samples.length()) { conoutf("unregistered sound: %d", n); return; };

    if(!samples[n])
    {
        sprintf_sd(buf)("packages/sounds/%s.wav", snames[n]);

        #ifdef USE_MIXER
            samples[n] = Mix_LoadWAV(path(buf));
        #else
            samples[n] = FSOUND_Sample_Load(n, path(buf), 0, 0);
        #endif

        if(!samples[n]) { /*conoutf("failed to load sample: %s", &buf);*/ return; };
    };
    int vol = soundvol;
    if(loc)
    {
        vol -= (int)(player1->o.dist(*loc)*3/4*soundvol/255);     // simple mono distance attenuation
    }
    if(vol<=0) return;

    #ifdef USE_MIXER
        int chan = Mix_PlayChannel(-1, samples[n], 0);
        if(chan>=0) Mix_Volume(chan, (vol*MAXVOL)/255);
    #else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, samples[n], NULL, true);
        if(chan>=0) { FSOUND_SetVolume(chan, (vol*MAXVOL)/255); FSOUND_SetPaused(chan, false); };
    #endif
};

void sound(int n) { playsound(n, NULL); };
COMMAND(sound, ARG_1INT);

