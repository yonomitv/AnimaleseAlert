// Created by yonomitv

#include "alert_app.h"
#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include "clayman.hpp"
#include "../include/clay_renderer_SDL3.c"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <format>
#include <deque>

// if defined, enables debug prints
//#define DEBUG_ENABLED

constexpr auto DIALOGUE_FONT_SIZE = 42;
constexpr auto DIALOGUE_FONT_LINEHEIGHT = 64;

// You can modify the color if you want
// {red, green, blue, alpha}
// all values 0-255 inclusive
constexpr Clay_Color DIALOGUE_COLOR = {150, 123, 59, 255};

// Note, this tries to sync the FPS to your monitor's refresh rate
// if you have issues, set this to false and adtjust FPS_CAP if you need to
constexpr bool USE_VSYNC = true;

// the frame cap is a little off, but it's roughly around +- 1ms
constexpr auto FPS_CAP = 60;
constexpr double FRAME_TIME = 1000.0 / FPS_CAP;

// if you wanna control volume through here, 0 is silent, 1 is original volume
// For your sake, I clamp this volume to a maximum of 2.
// 1 is pretty loud IMO
constexpr float AUDIO_VOLUME = 0.2f;

constexpr double FADE_IN_TIME = 3000;
constexpr double FADE_OUT_TIME = 3000;

// the time in milliseconds for the app to check for new alerts
constexpr auto CHECK_FOR_NEW_ALERT_INTERVAL = 5000;

// hard coded resolution matching the speech bubble image
constexpr auto IMAGE_WIDTH = 1006;
constexpr auto IMAGE_HEIGHT = 309;



// audio queue has each {id}_F.txt file path in it which we derive the other files
// For each redemption it can be a number of items in queue
// each audio file has a transcript and is loaded into transcriptdata when it is actively playing
std::deque<std::filesystem::path> AudioQueue;


struct TranscriptDatum {
    char FirstChar;
    char SecondChar = '\0';
    float Duration = 0;
};

enum class EAppState 
{
    Idle,
    FadeIn,
    StartPlayingAlert,
    PlayingAlert,
    FadeOut,
    DoNotPlayAlerts
};

// Only contains the transcript data for one audio file at a time unlike audioqueue.
std::vector<TranscriptDatum> TranscriptData;
// global used for text on screen
std::string DialogueText = "";

//struct AnimaleseDatum {
//    std::string Filename;
//};


// note, because this is calloc'd, pretty sure everything is just 0 anyways
// so look at the init function
typedef struct app_state {
    ClayMan clayMan;
    Clay_SDL3RendererData rendererData;
    Clay_RenderCommandArray renderCommands;
    float mouseX = 0;
    float mouseY = 0;
    float scrollX = 0;
    float scrollY = 0;
    int width = -1;
    int height = -1;
    SDL_Texture* sample_image;
    SDL_Window* window;
    bool mouseDown = false;
    Uint64 NOW = 1;
    Uint64 LAST = 0;
    double deltaTime = 1;
    MIX_Mixer* gMixer = NULL;
    MIX_Track* gMusic = NULL;
    //Used for fading in and out the speech bubble
    double AccumlatedTimeMS = 0;
    double TargetAccumlatedTimeMS = 0;
    bool bRenderDirty = true;
    float DialogueScrollY;
    int TotalLines = 0;
    size_t TranscriptIndex = 0;
    EAppState AppStateEnum;
    int file_id = 0;
    bool bNoMoreFiles = false;

} AppState;

// Reads Transcript file and loads it into TranscriptData
// Append only to TranscriptData struct, no clearing is done.
// Intention is to call this for each transcript file which could be multiple for one alert.
void ReadTranscript(const std::string& FilePath)
{
#ifdef DEBUG_ENABLED
    std::cout << "ReadTranscript() " << FilePath << std::endl;
#endif
    std::ifstream TFile(FilePath);
    std::string line;
    if (TFile.is_open()) {
        // reading two lines at a time.  
        // First line / even line is the character
        // second line is the time spent on that character
        //TranscriptData.clear();
        bool bEven = true;
        TranscriptDatum ACharDatum;
        while (std::getline(TFile, line)) {
            
            if (bEven)
            {
                ACharDatum = TranscriptDatum();
                //std::cout << line << " EVEN" << " line length =" << line.length() << std::endl;
                if (line.length() == 1)
                {
                    //if (line[0] == ' ')
                    //{
                    //    std::cout << "EMPTY SPACE \n";
                    //}
                    ACharDatum.FirstChar = line[0];
                }
                else if (line.length() == 2)
                {
                    ACharDatum.FirstChar = line[0];
                    ACharDatum.SecondChar = line[1];
                }
                bEven = false;
            }
            else
            {
                bEven = true;
                // animalese outputs milliseconds. usually round, but could be decimal
                float duration = std::stof(line);
                //std::cout << "duration: " << duration << "\n";
                ACharDatum.Duration = duration;
                TranscriptData.push_back(ACharDatum);
                //std::cout << duration << " ODD" << std::endl;
            }
        }
        TFile.close();
        // animalese.py inserts an extra space at the end. Remove it. 
        // So that we don't have extra scrolling. 
        if (TranscriptData.back().FirstChar == ' ')
        {
            TranscriptData.pop_back();
        }
    } else {
        std::cerr << "Unable to open file" << std::endl;
    }
}

// Processes one Datum in transcript data
// Adds one character (could be 2 if a digraph)
// Adds to TargetAccumlatedTimeMS 
// TargetAccumlatedTimeMS is used in AppIterate to know when to call this again 
bool ProcessTranscriptData(void* appstate)
{
    //std::cout << "ProcessTranscriptData\n";
    AppState* state = (AppState*) appstate;
    if (state->TranscriptIndex >= 0 && state->TranscriptIndex < TranscriptData.size())
    {
        // tricky
        std::string TempString;
        if (TranscriptData[state->TranscriptIndex].SecondChar == '\0')
        {
            TempString = DialogueText + 
                         TranscriptData[state->TranscriptIndex].FirstChar;
        }
        else

        {
            TempString = DialogueText + 
                         TranscriptData[state->TranscriptIndex].FirstChar + 
                         TranscriptData[state->TranscriptIndex].SecondChar;
        }
 
        DialogueText = TempString;
        // This method leads to audio finishing earlier.
        //state->TargetAccumlatedTimeMS = state->AccumlatedTimeMS + TranscriptData[state->TranscriptIndex].Duration;
        state->TargetAccumlatedTimeMS += TranscriptData[state->TranscriptIndex].Duration;
        state->TranscriptIndex += 1;
#ifdef DEBUG_ENABLED
        std::cout << DialogueText << std::endl;
#endif
        return true;
    }
    else
    {
        //we at the end.  handle?
        return false;
    }
}

/*
* There are 4 files that python will create when it is finished creating the alert
* 1. The audio file which should be the redemption id with a .wav extension
* 2. The transcript file which includes timestamps for this app. Same name, but .txt extension
* 3. A text file indicating that python is done processing. Here to avoid a race condition
*    Specifically for this Speech-to-Text version, assume many audio files can be created
*    Each audio file has a related f file to indicate that the audio file is completely written
*    1_f.txt, 2_f.txt, ..., n_f.txt files.  
*    Wanted to avoid having to use networking, so we're doing it like this.
* 4. finished.txt.  This file indicates that the redemption duration is over
*    That means we play the rest that we have, and then we start to fade out, but that's not here.
*   
*/
void CheckForNewAudio(void* appstate)
{
    AppState* state = (AppState*) appstate;

    // for performance, we stop checking since no more new files should be detected
    if (state->bNoMoreFiles) return;
       

    //std::string 
    for (const auto & entry : std::filesystem::directory_iterator("NewAudio"))
    {
        std::error_code ec; // For using the non-throwing overloads of functions below.
        if (!std::filesystem::is_regular_file(entry.path(), ec)) continue;

        std::string FileStem = entry.path().stem().string();
        std::string Extension = entry.path().extension().string();

        // This means a new redemption has started and to start the fade in.
        if (FileStem == "start")
        {
            std::filesystem::rename(std::filesystem::current_path() / "NewAudio" / "start.txt", std::filesystem::current_path() / "CompletedAudio" / "start.txt");
            state->AppStateEnum = EAppState::FadeIn;
            state->AccumlatedTimeMS = 0;
            state->file_id = 0;
        }
        // looking for the next completed audio file that is not added yet to AudioQueue
        else if (FileStem.ends_with("_F") && FileStem.starts_with(std::to_string(state->file_id)))
        {
            AudioQueue.push_back(entry.path());
            state->file_id += 1;
        }
        // Redemption duration finished
        else if (FileStem == "finished")
        {
            state->bNoMoreFiles = true;
            // this means we're done done.  we just cut off with what we got and finish.
        }
    }
}

// Move everything to completedaudio folder.
// Python script will then delete everything in completedaudio
void CleanUpLastAlertFiles()
{
#ifdef DEBUG_ENABLED
    std::cout << "CleanUpLastAlertFiles() " << std::filesystem::current_path() << std::endl;
#endif

    for (const auto & entry : std::filesystem::directory_iterator("NewAudio"))
    {
        std::filesystem::rename(std::filesystem::current_path() / "NewAudio" / entry.path().filename(), std::filesystem::current_path() / "CompletedAudio" / entry.path().filename());
    }
    //AudioQueue.erase(AudioQueue.begin());
    AudioQueue.clear();
}



static inline Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData)
{
    TTF_Font** fonts = (TTF_Font**)userData;
    TTF_Font* font = fonts[config->fontId];
    int width, height;
    //TTF_SetFontSize(font, config->fontSize);
    if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s", SDL_GetError());
    }

    return CLAY__INIT(Clay_Dimensions) { (float) width, (float) height };
}


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]){


    (void) argc;
    (void) argv;

    SDL_SetHint(SDL_HINT_LOGGING, "0");

    if (!TTF_Init()) {
        return SDL_APP_FAILURE;
    }

    AppState* state = (AppState*)SDL_calloc(1, sizeof(AppState));
    if (!state) {
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    if (!SDL_CreateWindowAndRenderer("Animalese Alert App", IMAGE_WIDTH, IMAGE_HEIGHT, SDL_WINDOW_TRANSPARENT, &state->window, &state->rendererData.renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    //SDL_SetWindowResizable(state->window, true);
    //SDL_SetWindowHitTest(state->window, window_hit_test, NULL);

    if (USE_VSYNC && !SDL_SetRenderVSync(state->rendererData.renderer, 1))
    {
        std::cout<< "Vsync failed to enable" << std::endl;
    }
#ifdef DEBUG_ENABLED
    std::cout << SDL_GetRenderDriver(0);
#endif

	//Initialize SDL_mixer
	if( !MIX_Init() )
	{
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_mixer could not initialize! SDL_mixer Error: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
	}
	else
	{
		state->gMixer = MIX_CreateMixerDevice( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL );
		if (!state->gMixer)
		{
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SDL_mixer could not create mixer! SDL_mixer Error: %s\n", SDL_GetError());
            return SDL_APP_FAILURE;
		}
        else
        {
            // clamped audio volume for safety
            MIX_SetMasterGain(state->gMixer, std::max(std::min(AUDIO_VOLUME, 2.f), 0.f));
        }
	}

    state->rendererData.textEngine = TTF_CreateRendererTextEngine(state->rendererData.renderer);
    if (!state->rendererData.textEngine) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create text engine from renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->rendererData.fonts = (TTF_Font**)SDL_calloc(2, sizeof(TTF_Font *));
    if (!state->rendererData.fonts) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to allocate memory for the font array: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    FILE* FontFile = fopen("resources/Aileron-Heavy.otf", "rb");
    
    fseek(FontFile, 0, SEEK_END);
    long FontSize = ftell(FontFile);
    fseek(FontFile, 0, SEEK_SET);
#ifdef DEBUG_ENABLED
    std::cout << "Font size " << FontSize << std::endl;
#endif
    char* FontBuffer = static_cast<char*>(malloc(FontSize + 1));
    if (FontBuffer == NULL)
    {
        std::cout<< "FAILED TO LOAD FONT FILE" << std::endl;
        return SDL_APP_FAILURE;
    }
    size_t bytesRead = fread(FontBuffer, 1, FontSize, FontFile);
#ifdef DEBUG_ENABLED
    std::cout << "Bytes read " << bytesRead;
#endif
    fclose(FontFile);
    SDL_IOStream* file_rw = SDL_IOFromConstMem(FontBuffer, bytesRead);
    

    state->rendererData.fonts[0] = TTF_OpenFontIO(file_rw, true, 11);
    state->rendererData.fonts[1] = TTF_OpenFontIO(file_rw, true, DIALOGUE_FONT_SIZE);

    state->sample_image = IMG_LoadTexture(state->rendererData.renderer, "resources/speech_bubble5.png");
    state->rendererData.alpha = 1;

    int width, height;
    SDL_GetWindowSize(state->window, &width, &height);
#ifdef DEBUG_ENABLED
    std::cout << "MIN CLAY: " << Clay_MinMemorySize() << std::endl;
#endif
    state->clayMan = ClayMan(width, height, SDL_MeasureText, state->rendererData.fonts);

    state->width = width;
    state->height = height;
    state->clayMan.updateClayState(state->width, state->height, state->mouseX, state->mouseY, state->scrollX, state->scrollY, 0.01f, state->mouseDown); 
    

    state->NOW = SDL_GetPerformanceCounter();
    state->LAST = 0;
    state->deltaTime = 0;

    state->AppStateEnum = EAppState::DoNotPlayAlerts;
    state->bRenderDirty = true;

    *appstate = state;
    //Clay_SetDebugModeEnabled(true);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event){
    AppState *state = (AppState*) appstate;
    SDL_AppResult ret_val = SDL_APP_CONTINUE;

    switch (event->type) {
        case SDL_EVENT_QUIT:
            ret_val = SDL_APP_SUCCESS;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            state->width = event->window.data1;
            state->height = event->window.data2;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            state->mouseX = event->motion.x;
            state->mouseY = event->motion.y;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(event->button.button == SDL_BUTTON_LEFT){
                state->mouseDown = true;
                //Clay_ScrollContainerData ContainerData = Clay_GetScrollContainerData(state->clayMan.getClayElementId("DialogueBubble"));
                //state->AppStateEnum = EAppState::FadeIn;
                if (state->AppStateEnum == EAppState::DoNotPlayAlerts)
                {
                    state->AppStateEnum = EAppState::Idle;
                    state->rendererData.alpha = 0;
                    state->bRenderDirty = true;
                }
                else if (state->AppStateEnum == EAppState::Idle)
                {
                    state->AppStateEnum = EAppState::DoNotPlayAlerts;
                    state->rendererData.alpha = 1;
                    state->bRenderDirty = true;
                }
                // purely for debug
                //CheckForNewAudio(state);
                //for (size_t i = 0; i < state->renderCommands.length; ++i)
                //{
                //    Clay_RenderCommand* rcmd = Clay_RenderCommandArray_Get(&state->renderCommands, i);
                //    //std::cout << rcmd->id;
                //    switch(rcmd->commandType)
                //    {
                //    case Clay_RenderCommandType::CLAY_RENDER_COMMAND_TYPE_TEXT:
                //        //std::cout << " text command\n";
                //        break;
                //    case Clay_RenderCommandType::CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                //        std::cout << "Border " << "\n";
                //        break;
                //    default:
                //        std::cout << rcmd->commandType << "\n";
                //    }
                //}
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(event->button.button == SDL_BUTTON_LEFT){
                state->mouseDown = false;
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (state->AppStateEnum == EAppState::DoNotPlayAlerts)
            {
                state->scrollX = event->wheel.x;
                state->scrollY = event->wheel.y;
                //Clay_ScrollContainerData ContainerData = Clay_GetScrollContainerData(state->clayMan.getClayElementId("DialogueBubble"));
                //std::cout << "x=" << ContainerData.scrollPosition->x << " y=" << ContainerData.scrollPosition->y << "\n";
                state->DialogueScrollY += state->scrollY;
                state->bRenderDirty = true;
            }
            break;
        default:
            break;
    };

    return ret_val;
}

void myLayout(void* appstate){
    AppState* state = (AppState*) appstate;
    ClayMan& clayMan = state->clayMan;

    Clay_TextElementConfig textConfig = {
        .textColor = {0, 0, 0, 255},
        .fontId = 0,
        .fontSize = 11
    };
    Clay_TextElementConfig textConfig2 = {
        .textColor = DIALOGUE_COLOR,
        .fontId = 1,
        .fontSize = DIALOGUE_FONT_SIZE,
        .lineHeight = DIALOGUE_FONT_LINEHEIGHT
    };

    clayMan.openElement({
        .id = clayMan.hashID("OuterContainer"),
        .layout = {
            .sizing = clayMan.fixedSize(IMAGE_WIDTH, IMAGE_HEIGHT),
            //.padding = clayMan.padAll(16),
            //.childGap = 16,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .image = {
            .imageData = state->sample_image
        }
    });
    {
        clayMan.openElement({
            .id = clayMan.hashID("DialogueBubblecont"),
            .layout = {
                .sizing = clayMan.expandXY(),
                .padding = Clay_Padding {100,4,72,4},
                //.childGap = 16,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
        });
        {
            clayMan.openElement({
                .id = clayMan.hashID("DialogueBubble"),
                .layout = {
                    .sizing = clayMan.fixedSize(784, 192),
                    //.padding = Clay_Padding {4,4,100,4},
                    //.childGap = 16,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                },
                .clip = {.vertical = true, .childOffset = 
                            (state->AppStateEnum == EAppState::PlayingAlert || state->AppStateEnum == EAppState::FadeOut) ?
                            Clay_Vector2(0, -std::max((state->TotalLines - 3), 0) * DIALOGUE_FONT_LINEHEIGHT) :
                            Clay_Vector2(0, -state->DialogueScrollY)
                        }
            });
            {
                if (state->AppStateEnum == EAppState::DoNotPlayAlerts)
                {
                    clayMan.textElement("Alert app is currently paused. No redemptions will be processed. To unpause, click somewhere on this screen. If the app turns completely transparent, then the app is running. To pause again, click again, while no alert is being played.", textConfig);
                    clayMan.textElement("There will be a 5 second delay between alerts which will allow you to pause alerts from being played and fulfilled", textConfig);
                    clayMan.textElement("Alternatively, just close this app. before it finishes playing. If the files are still in the NewAudio folder, then they will be replayed the next time this runs", textConfig);
                    clayMan.textElement("Once the files are in completed audio folder, if the python app is running, it will mark the channel redemptions as redeemed", textConfig);
                    clayMan.textElement("How it works:", textConfig);
                    clayMan.textElement("Animalese.py polls the twitch api for the matching channel redemption", textConfig);
                    clayMan.textElement("Once an unfulfilled channel redemption is seen, animalese generates the audio file (and more) and puts it in the NewAudio folder", textConfig);
                    clayMan.textElement("alert_app if running and not paused, periodically checks the folder. It sees the new audio file and plays the alert", textConfig);
                    clayMan.textElement("Alert_app moves the files to CompletedAudio after it's done", textConfig);
                    clayMan.textElement("Python sees that the files were moved and tells twitch the matching channel redemption was fulfilled", textConfig);
                }
                else
                {
                    clayMan.textElement(DialogueText, textConfig2);
                }
            } clayMan.closeElement();
        } clayMan.closeElement();
    } clayMan.closeElement();
    
}



SDL_AppResult SDL_AppIterate(void* appstate){
    AppState* state = (AppState*) appstate;

    state->LAST = state->NOW;
    state->NOW = SDL_GetPerformanceCounter();
    state->deltaTime = (double)((state->NOW - state->LAST)*1000 / (double)SDL_GetPerformanceFrequency() );
    //float fps = (float)(state->NOW - state->LAST) / (float)SDL_GetPerformanceFrequency();
    //std::cout << (1.0f / fps) << "\n";
    //std::cout << state->deltaTime << std::endl;
    state->clayMan.updateClayState(state->width, state->height, state->mouseX, state->mouseY, state->scrollX, -state->DialogueScrollY, state->deltaTime, state->mouseDown); 
    //state->DialogueScrollY = 0;
    //std::cout << state->scrollY << std::endl;
    state->scrollX = 0;
    state->scrollY = 0;
    

    if (state->AppStateEnum == EAppState::Idle)
    {
        // this checks the NewAudio directory and updates AudioQueue if anything new
        
        state->AccumlatedTimeMS += state->deltaTime;

        // we only check every 5 seconds for new alerts to play.
        // Ideally gives user time to pause if they need to.  
        if (state->AccumlatedTimeMS >= CHECK_FOR_NEW_ALERT_INTERVAL)
        {
        #ifdef DEBUG_ENABLED
            std::cout << "Checking for new audio\n";
        #endif
            CheckForNewAudio(state);
            state->AccumlatedTimeMS = 0;
        }
    }
    else if (state->AppStateEnum == EAppState::FadeIn)
    {
        if (state->AccumlatedTimeMS < FADE_IN_TIME)
        {
            //std::cout << "AccumlatedTimeMS = " << state->AccumlatedTimeMS << "\n";
            //std::cout << "alpha = " << state->rendererData.alpha << "\n";
            state->AccumlatedTimeMS += state->deltaTime;
            // This is just a clamp btw
            state->rendererData.alpha = ((std::max(std::min(state->AccumlatedTimeMS, FADE_IN_TIME), 0.0)) / FADE_IN_TIME);
        }
        else //state->AccumlatedTimeMS >= 1000
        {
            // let's python script know that we've finished fading in so that it can start.  
            std::ofstream file("NewAudio/fadeindone.txt");
            if (!file.is_open())
            {
                std::cout << "File cannot be created";
                return SDL_APP_SUCCESS;
            }
            state->AccumlatedTimeMS = 0;
            state->rendererData.alpha = 1;
            state->AppStateEnum = EAppState::StartPlayingAlert;
        }
    }
    else if (state->AppStateEnum == EAppState::StartPlayingAlert)
    {
        //std::cout << "Start playing alert \n";
        CheckForNewAudio(state);
        if (AudioQueue.size() > 0)
        {
            // reset some vars
            DialogueText = "";
            state->TranscriptIndex = 0;

            std::string FileStem = AudioQueue[0].stem().string();
            std::string AudioFileStem = "";

            // we have the 1_F.txt file stored, now we're stripping it to reconstruct the .wav
            for (size_t i = 0; i < FileStem.length() - 2; ++i)
            {
                AudioFileStem += FileStem[i];
            }
        
            std::string path_str = std::format("{}/{}.wav", "NewAudio", AudioFileStem);
            std::string transcript_path_str = std::format("{}/{}.txt", "NewAudio", AudioFileStem);
            const char* path_c_str = path_str.c_str();

            // Transcript data is loaded per audio being played so we clear before each one.
            TranscriptData.clear();
            ReadTranscript(transcript_path_str);
        
	        MIX_Audio* music = MIX_LoadAudio( state->gMixer, path_c_str, false );
	        if( music == NULL )
	        {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load beat music! SDL_mixer Error: %s\n", SDL_GetError());
	        }
            else
            {
		        state->gMusic = MIX_CreateTrack( state->gMixer );
		        if( state->gMusic == NULL )
		        {
                    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create music track! SDL_mixer Error: %s\n", SDL_GetError());
		        }
	            else
	            {
		            MIX_SetTrackAudio( state->gMusic, music );
	            }
            #ifdef DEBUG_ENABLED
                std::cout << "New Audio was loaded in \n";
            #endif
		        MIX_DestroyAudio( music );
                state->AppStateEnum = EAppState::FadeIn;
                state->AccumlatedTimeMS = 0;
            }



	        if (state->gMusic)
	        {
		        //Play the music
            #ifdef DEBUG_ENABLED
                std::cout << "we playing track";
            #endif
		        SDL_PropertiesID props = SDL_CreateProperties();
		        //SDL_SetNumberProperty( props, MIX_PROP_PLAY_LOOPS_NUMBER, -1 );
		        MIX_PlayTrack( state->gMusic, props );
		        SDL_DestroyProperties( props );
	        }
            else
            {
                std::string debug_str = std::format("This should never happen. IsTrackPlaying={}\n", MIX_TrackPlaying( state->gMusic ));
                std::cout << debug_str;
            }
            state->AppStateEnum = EAppState::PlayingAlert;
            state->AccumlatedTimeMS = 0;
            state->TargetAccumlatedTimeMS = 0;



            ProcessTranscriptData(state);
            // basically we need this if there's for some reason empty spaces at the start
            while (std::fabs(state->AccumlatedTimeMS - state->TargetAccumlatedTimeMS) <= 0.1)
            {
                // In case we only encounter spaces
                if (!ProcessTranscriptData(state))
                {
                    // I don't think we'd need this extra space
                    // but just in case idk.
                    DialogueText += " ";
                    // remove this audio from queue.
                    // We don't remove unless we are completely finished with audio and transcript
                    AudioQueue.pop_front();
                    break;
                }
            }
        }
    }
    else if (state->AppStateEnum == EAppState::PlayingAlert)
    {
        CheckForNewAudio(state);
        //std::cout<< "PLAYING ALERT\n";
        state->AccumlatedTimeMS += state->deltaTime;
        //std::cout << state->TranscriptIndex << std::endl;
        //std::cout << "Current Text: " << DialogueText << std::endl;

        // We will hit this once we get a new audio file
        if (AudioQueue.size() > 0 && 
            state->AccumlatedTimeMS >= state->TargetAccumlatedTimeMS)
        {
            bool bSuccess = ProcessTranscriptData(state);
            // 2 cases we wanna keep processing the transcript data repeatedly
            //   1. There's an empty space. This is purely for visual so there's no time to accumulate.
            //   2. The time accumulated would include more than just one character
            // this way, we keep the audio and text in sync as it writes out on screen.
            while (std::fabs(state->AccumlatedTimeMS - state->TargetAccumlatedTimeMS) <= 0.1 || 
                (state->AccumlatedTimeMS > 0.1 && state->AccumlatedTimeMS - state->TargetAccumlatedTimeMS >= 0.1))
            {
                // In case we only encounter spaces
            #ifdef DEBUG_ENABLED
                std::cout << "ProcessTranscriptData " << state->AccumlatedTimeMS << " " << state->TargetAccumlatedTimeMS << "\n";
            #endif
                bSuccess = ProcessTranscriptData(state);
                if (!bSuccess)
                {
                    break;
                }
            }
            // TODO: check for more audio files to play (and transcripts)
            if (!bSuccess)
            {
            #ifdef DEBUG_ENABLED
                std::cout<< "Reloading\n";
            #endif
                state->AccumlatedTimeMS = 0;
                state->TargetAccumlatedTimeMS = 0;

                // the firs time we fail on processing transcript
                // so we know we're finished
                if (TranscriptData.size() > 0 && 
                    TranscriptData.size() == state->TranscriptIndex)
                {
                    // remove this audio from queue.
                    // We don't remove unless we are completely finished with audio and transcript
                    AudioQueue.pop_front();
                    // Transcript data is loaded per audio being played so we clear before each one.
                    TranscriptData.clear();
                }
                else
                {
                    // reset some vars
                    //DialogueText = "";
                    state->TranscriptIndex = 0;


                    std::string FileStem = AudioQueue[0].stem().string();
                    std::string AudioFileStem = "";

                    // we have the 1_F.txt file stored, now we're stripping it to reconstruct the .wav
                    for (size_t i = 0; i < FileStem.length() - 2; ++i)
                    {
                        AudioFileStem += FileStem[i];
                    }
        
                    std::string path_str = std::format("{}/{}.wav", "NewAudio", AudioFileStem);
                    std::string transcript_path_str = std::format("{}/{}.txt", "NewAudio", AudioFileStem);
                    const char* path_c_str = path_str.c_str();


                    ReadTranscript(transcript_path_str);
        
	                MIX_Audio* music = MIX_LoadAudio( state->gMixer, path_c_str, false );
	                if( music == NULL )
	                {
                        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load beat music! SDL_mixer Error: %s\n", SDL_GetError());
	                }
                    else
                    {
		                state->gMusic = MIX_CreateTrack( state->gMixer );
		                if( state->gMusic == NULL )
		                {
                            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create music track! SDL_mixer Error: %s\n", SDL_GetError());
		                }
	                    else
	                    {
		                    MIX_SetTrackAudio( state->gMusic, music );
	                    }
                    #ifdef DEBUG_ENABLED
                        std::cout << "New Audio was loaded in \n";
                    #endif
		                MIX_DestroyAudio( music );
                        state->AppStateEnum = EAppState::FadeIn;
                        state->AccumlatedTimeMS = 0;
                    }



	                if (state->gMusic)
	                {
		                //Play the music
                    #ifdef DEBUG_ENABLED
                        std::cout << "we playing track";
                    #endif
		                SDL_PropertiesID props = SDL_CreateProperties();
		                //SDL_SetNumberProperty( props, MIX_PROP_PLAY_LOOPS_NUMBER, -1 );
		                MIX_PlayTrack( state->gMusic, props );
		                SDL_DestroyProperties( props );
	                }
                    else
                    {
                        std::string debug_str = std::format("This should never happen. IsTrackPlaying={}\n", MIX_TrackPlaying( state->gMusic ));
                        std::cout << debug_str;
                    }
                    state->AppStateEnum = EAppState::PlayingAlert;
                    state->AccumlatedTimeMS = 0;
                    state->TargetAccumlatedTimeMS = 0;

                    // add a space to separate things
                    // proper punctuation would be difficult.  
                    DialogueText += " ";



                    ProcessTranscriptData(state);
                    // basically we need this if there's for some reason empty spaces at the start
                    while (std::fabs(state->AccumlatedTimeMS - state->TargetAccumlatedTimeMS) <= 0.1)
                    {
                        // In case we only encounter spaces
                        if (!ProcessTranscriptData(state))
                        {
                            break;
                        }

                    }
                }
            }
        }
        // we've received the finished signal from python script
        // Finish playing the rest of the existing audio
        // if no more audio to play, then set to fadeout.  
        else if (state->bNoMoreFiles && AudioQueue.size() == 0)
        {
            state->AppStateEnum = EAppState::FadeOut;
        }
    }
    else if (state->AppStateEnum == EAppState::FadeOut)
    {
        //std::cout << "NUmlines " << state->TotalLines << std::endl;
        if (state->AccumlatedTimeMS < FADE_OUT_TIME)
        {
            //std::cout << "AccumlatedTimeMS < 1000 \n";
            state->AccumlatedTimeMS += state->deltaTime;
            if (state->AccumlatedTimeMS >= 1000)
            {
                // clamp it between 0 and 1
                state->rendererData.alpha = 1 - ((std::max(std::min(state->AccumlatedTimeMS - 1000, FADE_OUT_TIME - 1000), 0.0)) / (FADE_OUT_TIME - 1000));
            }
        }
        else //state->AccumlatedTimeMS >= 1000
        {
        #ifdef DEBUG_ENABLED
            std::cout << "Fade out Ended\n";
        #endif
            state->AccumlatedTimeMS = 0;
            state->rendererData.alpha = 0;
            DialogueText = "";
            state->AppStateEnum = EAppState::Idle;
            state->gMusic = NULL;
            state->TotalLines = 0;
            state->bNoMoreFiles = false;
            CleanUpLastAlertFiles();
            // let's python script know that we've finished fading out so that it can check for new redeems.  
            std::ofstream file("NewAudio/fadeoutdone.txt");
            if (!file.is_open())
            {
                std::cout << "File cannot be created";
                return SDL_APP_SUCCESS;
            }
        }
    }

    state->clayMan.beginLayout();
    myLayout(state);
    state->renderCommands = state->clayMan.endLayout();

    if (state->AppStateEnum == EAppState::PlayingAlert)
    {
        int NumLines = 0;
        for (size_t i = 0; i < state->renderCommands.length; ++i)
        {
            Clay_RenderCommand* rcmd = Clay_RenderCommandArray_Get(&state->renderCommands, i);
            //std::cout << rcmd->id;
            switch(rcmd->commandType)
            {
            case Clay_RenderCommandType::CLAY_RENDER_COMMAND_TYPE_TEXT:
                //std::cout << " text command\n";
                NumLines += 1;
                break;
            //default:
                //std::cout << rcmd->commandType << "\n";
            }
        }
        if (NumLines > state->TotalLines)
        {
            state->TotalLines = NumLines;

            // not ideal doing this twice, but it's simple
            state->clayMan.beginLayout();
            myLayout(state);
            state->renderCommands = state->clayMan.endLayout();
        }
        
    }

    SDL_SetRenderDrawColor(state->rendererData.renderer, 0, 0, 0, 0);
    SDL_RenderClear(state->rendererData.renderer);
    SDL_Clay_RenderClayCommands(&state->rendererData, &state->renderCommands);
    SDL_RenderPresent(state->rendererData.renderer);

    if (!USE_VSYNC)
    {
        // this is in seconds
        double elapsed_time = (double)((SDL_GetPerformanceCounter() - state->NOW) / (double)SDL_GetPerformanceFrequency() * 1000);
        double sleep_time = FRAME_TIME - elapsed_time;
        //std::cout << "sleep_time= " << sleep_time << std::endl;
        if (sleep_time > 0)
        {
            SDL_Delay(floor(sleep_time));
        }

    }

    // Trying to account for high refresh rate monitors with vsync on
    // Don't want cpu usage to be absurd
    // capped to 30 fps
    else if (state->AppStateEnum == EAppState::Idle || 
            state->AppStateEnum == EAppState::DoNotPlayAlerts)
    {
        SDL_DelayNS(33333320);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result){
    (void) result; if (result != SDL_APP_SUCCESS) {SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Application failed to run");}

    AppState* state = (AppState*) appstate;

    if (state) {
        if (state->rendererData.renderer) SDL_DestroyRenderer(state->rendererData.renderer);
        if (state->window) SDL_DestroyWindow(state->window);
        if (state->rendererData.textEngine) TTF_DestroyRendererTextEngine(state->rendererData.textEngine);
        SDL_free(state);
    }

    TTF_Quit();
}
