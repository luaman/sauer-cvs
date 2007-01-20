// sound.cpp: uses fmod on windows and sdl_mixer on unix (both had problems on the other platform)

#include "pch.h"
#include "engine.h"

//#ifndef WIN32    // NOTE: fmod not being supported for the moment as it does not allow stereo pan/vol updating during playback
#define USE_MIXER
//#endif

bool nosound = true;

#define MAXCHAN 32
#define SOUNDFREQ 22050

#ifdef USE_MIXER
    #include "SDL_mixer.h"
    #define MAXVOL MIX_MAX_VOLUME
    Mix_Music *mod = NULL;
    void *stream = NULL;    // TODO
#else
    #include "fmod.h"
    FMUSIC_MODULE *mod = NULL;
    FSOUND_STREAM *stream = NULL;

    #define MAXVOL 255
    int musicchan;
#endif

struct sample
{
    char *name;
    #ifdef USE_MIXER
        Mix_Chunk *sound;
    #else
        FSOUND_SAMPLE *sound;
    #endif

    sample() : name(NULL) {};
    ~sample() { DELETEA(name); };
};

struct soundslot
{
    sample *s;
    int vol;
};

struct soundloc { vec loc; bool inuse; soundslot *slot; extentity *ent; } soundlocs[MAXCHAN];

void setmusicvol(int musicvol)
{
    if(nosound) return;
    #ifdef USE_MIXER
        if(mod) Mix_VolumeMusic((musicvol*MAXVOL)/255);
    #else
        if(mod) FMUSIC_SetMasterVolume(mod, musicvol);
        else if(stream && musicchan>=0) FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255);
    #endif
};

VARP(soundvol, 0, 255, 255);
VARFP(musicvol, 0, 128, 255, setmusicvol(musicvol));

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
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
    if(stream)
    {
        #ifndef USE_MIXER
            FSOUND_Stream_Close(stream);
        #endif
        stream = NULL;
    };
};

VAR(soundbufferlen, 128, 1024, 4096);

void initsound()
{
    memset(soundlocs, 0, sizeof(soundloc)*MAXCHAN);
    #ifdef USE_MIXER
        if(Mix_OpenAudio(SOUNDFREQ, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
        {
            conoutf("sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
            return;
        };
	    Mix_AllocateChannels(MAXCHAN);	
    #else
        if(FSOUND_GetVersion()<FMOD_VERSION) fatal("old FMOD dll");
        if(!FSOUND_Init(SOUNDFREQ, MAXCHAN, FSOUND_INIT_GLOBALFOCUS))
        {
            conoutf("sound init failed (FMOD): %d", FSOUND_GetError());
            return;
        };
    #endif
    nosound = false;
};

void musicdone()
{
#ifdef USE_MIXER
    if(mod) Mix_FreeMusic(mod);
#else
    if(mod) FMUSIC_FreeSong(mod);
    if(stream) FSOUND_Stream_Close(stream);
#endif
    mod = NULL;
    stream = NULL;
    if(musicdonecmd)
    {
        char *cmd = musicdonecmd;
        musicdonecmd = NULL;
        execute(cmd);
        delete[] cmd;
    };
};

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(soundvol && musicvol)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);
        string sn;
        s_strcpy(sn, "packages/");
        s_strcat(sn, name);
        #ifdef USE_MIXER
            if((mod = Mix_LoadMUS(path(sn))))
            {
                Mix_PlayMusic(mod, cmd[0] ? 0 : -1);
                Mix_VolumeMusic((musicvol*MAXVOL)/255);
            }
        #else
            if((mod = FMUSIC_LoadSong(path(sn))))
            {
                FMUSIC_PlaySong(mod);
                FMUSIC_SetMasterVolume(mod, musicvol);
                FMUSIC_SetLooping(mod, cmd[0] ? FALSE : TRUE);
            }
            else if((stream = FSOUND_Stream_Open(path(sn), cmd[0] ? FSOUND_LOOP_OFF : FSOUND_LOOP_NORMAL, 0, 0)))
            {
                musicchan = FSOUND_Stream_Play(FSOUND_FREE, stream);
                if(musicchan>=0) { FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255); FSOUND_SetPaused(musicchan, false); };
            }
        #endif
            else
            {
                conoutf("could not play music: %s", sn);
            };
    };
};

COMMAND(music, "ss");

hashtable<char *, sample> samples;
vector<soundslot> gamesounds, mapsounds;

int findsound(char *name, int vol, vector<soundslot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].s->name, name) && (!vol || sounds[i].vol==vol)) return i;
    };
    return -1;
};

int addsound(char *name, int vol, vector<soundslot> &sounds)
{
    sample *s = samples.access(name);
    if(!s)
    {
        char *n = newstring(name);
        s = &samples[n];
        s->name = n;
        s->sound = NULL;
    };
    soundslot &slot = sounds.add();
    slot.s = s;
    slot.vol = vol ? vol : 100;
    return sounds.length()-1;
};

void registersound(char *name, int *vol) { intret(addsound(name, *vol, gamesounds)); };
COMMAND(registersound, "si");

void mapsound(char *name, int *vol) { intret(addsound(name, *vol, mapsounds)); };
COMMAND(mapsound, "si");

void clear_sound()
{
    if(nosound) return;
    stopsound();
    gamesounds.setsizenodelete(0);
    mapsounds.setsizenodelete(0);
    samples.clear();
    #ifdef USE_MIXER
        Mix_CloseAudio();
    #else
        FSOUND_Close();
    #endif
};

void clearmapsounds()
{
    loopi(MAXCHAN) if(soundlocs[i].inuse && soundlocs[i].ent)
    {
        #ifdef USE_MIXER
            if(Mix_Playing(i)) Mix_HaltChannel(i);
        #else
            if(FSOUND_IsPlaying(i)) FSOUND_StopSound();
        #endif
        soundlocs[i].inuse = false;
        soundlocs[i].ent->visible = false;
    };
    mapsounds.setsizenodelete(0);
};

void checkmapsounds()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type!=ET_SOUND || e.visible || camera1->o.dist(e.o)>=e.attr2) continue;
        playsound(e.attr1, NULL, &e);
    };
};

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, int svol, const vec *loc = NULL, extentity *ent = NULL)
{
    int vol = soundvol, pan = 255/2;
    if(loc)
    {
        vec v;
        float dist = camera1->o.dist(*loc, v);
        if(ent)
        {
            int rad = ent->attr2;
            if(ent->attr3)
            {
                rad -= ent->attr3;
                dist -= ent->attr3;
            };
            vol -= (int)(min(max(dist/rad, 0), 1)*soundvol);
        }
        else
        {
            vol -= (int)(dist*3/4*soundvol/255); // simple mono distance attenuation
            if(vol<0) vol = 0;
        };
        if(stereo && (v.x != 0 || v.y != 0))
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        };
    };
    vol = (vol*MAXVOL*svol)/255/255;
    #ifdef USE_MIXER
        Mix_Volume(chan, vol);
        Mix_SetPanning(chan, 255-pan, pan);
    #else
        FSOUND_SetVolume(chan, vol);
        FSOUND_SetPan(chan, pan);
    #endif
};  

void newsoundloc(int chan, const vec *loc, soundslot *slot, extentity *ent = NULL)
{
    ASSERT(chan>=0 && chan<MAXCHAN);
    soundlocs[chan].loc = *loc;
    soundlocs[chan].inuse = true;
    soundlocs[chan].slot = slot;
    soundlocs[chan].ent = ent;
};

void updatevol()
{
    if(nosound) return;
    loopi(MAXCHAN) if(soundlocs[i].inuse)
    {
        #ifdef USE_MIXER
            if(Mix_Playing(i))
        #else
            if(FSOUND_IsPlaying(i))
        #endif
                updatechanvol(i, soundlocs[i].slot->vol, &soundlocs[i].loc, soundlocs[i].ent);
            else 
            {
                soundlocs[i].inuse = false;
                if(soundlocs[i].ent) soundlocs[i].ent->visible = false;
            };
    };
#ifndef USE_MIXER
    if(mod && FMUSIC_IsFinished(mod)) musicdone();
    else if(stream && !FSOUND_IsPlaying(musicchan)) musicdone();
#else
    if(mod && !Mix_PlayingMusic()) musicdone();
#endif
};

int soundsatonce = 0, lastsoundmillis = 0;

void playsound(int n, const vec *loc, extentity *ent)
{
    if(nosound) return;
    if(!soundvol) return;
    if(lastmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
    lastsoundmillis = lastmillis;
    if(soundsatonce>5) return;  // avoid bursts of sounds with heavy packetloss and in sp
    vector<soundslot> &sounds = ent ? mapsounds : gamesounds;
    if(!sounds.inrange(n)) { conoutf("unregistered sound: %d", n); return; };

    soundslot &slot = sounds[n];
    if(!slot.s->sound)
    {
        s_sprintfd(buf)("packages/sounds/%s.wav", slot.s->name);

        #ifdef USE_MIXER
            slot.s->sound = Mix_LoadWAV(path(buf));
        #else
            slot.s->sound = FSOUND_Sample_Load(ent ? n+gamesounds.length() : n, path(buf), FSOUND_LOOP_OFF, 0, 0);
        #endif

        if(!slot.s->sound) { conoutf("failed to load sample: %s", buf); return; };
    };

    #ifdef USE_MIXER
        int chan = Mix_PlayChannel(-1, slot.s->sound, 0);
    #else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, slot.s->sound, NULL, true);
    #endif
    if(chan<0) return;
    if(ent)
    {
        loc = &ent->o;
        ent->visible = true;
    };
    if(loc) newsoundloc(chan, loc, &slot, ent);
    updatechanvol(chan, slot.vol, loc, ent);
    #ifndef USE_MIXER
        FSOUND_SetPaused(chan, false);
    #endif
};

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, gamesounds);
    playsound(id, loc);
};

void sound(int *n) { playsound(*n); };
COMMAND(sound, "i");

