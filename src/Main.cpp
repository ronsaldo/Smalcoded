#include "SDL.h"
#include "SDL_image.h"
#include "SDL_main.h"
#include "GameInterface.hpp"
#include "ControllerState.hpp"
#include <algorithm>

#define GAME_TITLE "Small eco destroyed world"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef USE_LIVE_CODING
#ifdef _WIN32
#error Implement livecoding support for windows
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>

#define LIBRARY_FILENAME(baseName) "./lib" baseName ".so"

inline void *loadLibrary(const char *name)
{
    auto result = dlopen(name, RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    //if(!result)
    //    fprintf(stderr, "Failed to load game logic library: %s\n", dlerror());
    return result;
}

inline void freeLibrary(void *handle)
{
    dlclose(handle);
}

inline void *getLibrarySymbol(void *handle, const char *symbol)
{
    return dlsym(handle, symbol);
}

typedef double FileTimestamp;

inline FileTimestamp getFileLastModificationTimestamp(const char *fileName)
{
    struct stat s;
    stat(fileName, &s);
    return double(s.st_mtim.tv_sec) + double(s.st_mtim.tv_nsec)*1e-9;
}

typedef void *LibraryHandle;

#endif
#endif

static int screenWidth = 640;
static int screenHeight = 480;
static MemoryZone persistentMemory;
static MemoryZone transientMemory;
static bool quitting = false;
static GameInterface *currentGameInterface;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;

static int gameControllerIndex;
static SDL_GameController *gameController;

static ControllerState oldKeyboardControllerState;
static ControllerState keyboardControllerState;
static ControllerState oldGamepadControllerState;
static ControllerState gamepadControllerState;
static ControllerState currentControllerState;

#ifdef USE_LIVE_CODING
static constexpr const char *GameLogicLibraryName = LIBRARY_FILENAME("SmallEcoDestroyedGameLogic");

static LibraryHandle libraryHandle;
static FileTimestamp lastLibraryModification;

static bool isLoadedLibraryOld()
{
    if(!libraryHandle)
        return true;

    return getFileLastModificationTimestamp(GameLogicLibraryName) > lastLibraryModification;
}

static void reloadGameInterface()
{
    if(isLoadedLibraryOld())
    {
        if(libraryHandle)
        {
            currentGameInterface = nullptr;
            freeLibrary(libraryHandle);
        }

        libraryHandle = loadLibrary(GameLogicLibraryName);
        if(!libraryHandle)
            return;

        //printf("Library loaded %p\n", libraryHandle);
        lastLibraryModification = getFileLastModificationTimestamp(GameLogicLibraryName);

        auto getGameInterface = reinterpret_cast<GetGameInterfaceFunction> (getLibrarySymbol(libraryHandle, "getGameInterface"));
        currentGameInterface = getGameInterface();
        currentGameInterface->setPersistentMemory(&persistentMemory);
        currentGameInterface->setTransientMemory(&transientMemory);
    }
}

#else
extern "C" GameInterface *getGameInterface();

void reloadGameInterface()
{
    currentGameInterface = getGameInterface();
}

#endif

static void onKeyEvent(const SDL_KeyboardEvent &event, bool isDown)
{
    switch(event.keysym.sym)
    {
    case SDLK_z:
        keyboardControllerState.setButton(ControllerButton::A, isDown);
        break;
    case SDLK_x:
        keyboardControllerState.setButton(ControllerButton::B, isDown);
        break;
    case SDLK_a:
        keyboardControllerState.setButton(ControllerButton::X, isDown);
        break;
    case SDLK_s:
        keyboardControllerState.setButton(ControllerButton::Y, isDown);
        break;
    case SDLK_q:
        keyboardControllerState.setButton(ControllerButton::LeftShoulder, isDown);
        break;
    case SDLK_w:
        keyboardControllerState.setButton(ControllerButton::RightShoulder, isDown);
        break;
    case SDLK_e:
        keyboardControllerState.setButton(ControllerButton::LeftTrigger, isDown);
        break;
    case SDLK_d:
        keyboardControllerState.setButton(ControllerButton::RightTrigger, isDown);
        break;
    case SDLK_LEFT:
        if(isDown)
            keyboardControllerState.leftXAxis = -1;
        else if(keyboardControllerState.leftXAxis < 0)
            keyboardControllerState.leftXAxis = 0;
        break;
    case SDLK_RIGHT:
        if(isDown)
            keyboardControllerState.leftXAxis = 1;
        else if(keyboardControllerState.leftXAxis > 0)
            keyboardControllerState.leftXAxis = 0;
        break;
    case SDLK_DOWN:
        if(isDown)
            keyboardControllerState.leftYAxis = -1;
        else if(keyboardControllerState.leftYAxis < 0)
            keyboardControllerState.leftYAxis = 0;
        break;
    case SDLK_UP:
        if(isDown)
            keyboardControllerState.leftYAxis = 1;
        else if(keyboardControllerState.leftYAxis > 0)
            keyboardControllerState.leftYAxis = 0;
        break;
    case SDLK_ESCAPE:
        keyboardControllerState.setButton(ControllerButton::Start, isDown);
        break;
    case SDLK_TAB:
        keyboardControllerState.setButton(ControllerButton::Select, isDown);
        break;
    case SDLK_r:
        if(isDown)
        {
            persistentMemory.reset();
            transientMemory.reset();
        }
        break;
#ifdef USE_LIVE_CODING
    case SDLK_F1:
        quitting = true;
        break;
#endif
    default:
        break;
    }

}

static void openGameController()
{
    if(gameController)
        return;

    auto joystickCount = SDL_NumJoysticks();
    gameControllerIndex = -1;
    for(auto i = 0; i < joystickCount; ++i)
    {
        if(SDL_IsGameController(i))
        {
            gameControllerIndex = i;
            break;
        }
    }

    gameController = SDL_GameControllerOpen(gameControllerIndex);

}

constexpr int AxisMinValue = -32768;
constexpr int AxisMaxValue = 32767;

inline int mapAxisValue(Sint16 value)
{
    if(value > AxisMaxValue / 2)
        return 1;
    else if(value < AxisMinValue / 2)
        return -1;
    else
        return 0;
}

inline bool mapTriggerValue(Sint16 value)
{
    return value > AxisMaxValue / 2;
}

inline int mapDigitalAxis(SDL_GameController *controller, SDL_GameControllerButton negative, SDL_GameControllerButton positive)
{
    if(SDL_GameControllerGetButton(controller, negative) != 0)
        return -1;
    else if(SDL_GameControllerGetButton(controller, positive) != 0)
        return 1;
    else
        return 0;
}

static void pollJoysticks()
{
    if(!gameController)
        return;

    gamepadControllerState.leftXAxis = mapAxisValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTX));
    gamepadControllerState.leftYAxis = -mapAxisValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_LEFTY));
    gamepadControllerState.rightXAxis = mapAxisValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTX));
    gamepadControllerState.rightYAxis = mapAxisValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_RIGHTY));

    if(gamepadControllerState.leftXAxis == 0)
        gamepadControllerState.leftXAxis = mapDigitalAxis(gameController, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    if(gamepadControllerState.leftYAxis == 0)
        gamepadControllerState.leftYAxis = mapDigitalAxis(gameController, SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_UP);

#define BUTTON_MAPPING(source, target) \
    gamepadControllerState.setButton(target, SDL_GameControllerGetButton(gameController, source) != 0)
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_A, ControllerButton::A);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_B, ControllerButton::B);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_X, ControllerButton::X);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_Y, ControllerButton::Y);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_BACK, ControllerButton::Select);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_START, ControllerButton::Start);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, ControllerButton::LeftShoulder);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, ControllerButton::RightShoulder);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_LEFTSTICK, ControllerButton::LeftStick);
    BUTTON_MAPPING(SDL_CONTROLLER_BUTTON_RIGHTSTICK, ControllerButton::RightStick);
#undef BUTTON_MAPPING
    gamepadControllerState.setButton(ControllerButton::LeftTrigger, mapTriggerValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERLEFT)));
    gamepadControllerState.setButton(ControllerButton::RightTrigger, mapTriggerValue(SDL_GameControllerGetAxis(gameController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)));
    //printf("joystickCount %d\n", joystickCount);
}

static void processEvents()
{
    oldKeyboardControllerState = keyboardControllerState;

    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            onKeyEvent(event.key, true);
            break;
        case SDL_KEYUP:
            onKeyEvent(event.key, false);
            break;
        case SDL_QUIT:
            quitting = true;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            //printf("Controller added: %d\n", event.cdevice.which);
            openGameController();
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            //printf("Controller removed: %d\n", event.cdevice.which);
            if(gameController == SDL_GameControllerFromInstanceID(event.cdevice.which))
            {
                SDL_GameControllerClose(gameController);
                gameController = nullptr;
                gameControllerIndex = -1;
            }
            break;
        case SDL_CONTROLLERDEVICEREMAPPED:
            //printf("Controller remapped\n");
            break;
        }
    }

    oldGamepadControllerState = gamepadControllerState;
    pollJoysticks();

    currentControllerState.applyDifferencesOf(oldKeyboardControllerState, keyboardControllerState);
    currentControllerState.applyDifferencesOf(oldGamepadControllerState, gamepadControllerState);
}

static void update(float timestep)
{
    if(currentGameInterface)
        currentGameInterface->update(timestep, currentControllerState);
}

static void render()
{
    uint8_t *backBuffer;
    int pitch;

    if(currentGameInterface)
    {
        SDL_LockTexture(texture, nullptr, reinterpret_cast<void**> (&backBuffer), &pitch);
        Framebuffer fb;
        fb.width = screenWidth;
        fb.height = screenHeight;
        fb.pixels = backBuffer;
        fb.pitch = pitch;
        currentGameInterface->render(fb);
        SDL_UnlockTexture(texture);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 0);
    SDL_RenderClear(renderer);
    if(currentGameInterface)
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

static float accumulatedTime;
static Uint32 lastUpdateTime;
static Uint32 frameRenderTime;
static Uint32 frameRenderCount;
static void mainLoopIteration()
{
    static constexpr float TimeStep = 1.0f/60.0f;

    reloadGameInterface();
    processEvents();

    // Compute the delta ticks.
    auto newUpdateTime = SDL_GetTicks();
    auto deltaTicks = newUpdateTime - lastUpdateTime;
    lastUpdateTime = newUpdateTime;

    // Accumulate the the time.
    accumulatedTime += deltaTicks * 0.001f;
    accumulatedTime = std::min(accumulatedTime, 5*TimeStep);
    auto iterationCount = 0;
    while(accumulatedTime >= TimeStep - 0.01f)
    {
        update(TimeStep);
        accumulatedTime -= TimeStep;
        ++iterationCount;
    }

    //if(iterationCount == 0)
    //    printf("Not iterated update %f\n", accumulatedTime);
    //else if(iterationCount > 1)
    //    printf("Multiples iterations %d\n", iterationCount);

    render();

    frameRenderTime += deltaTicks;
    ++frameRenderCount;
    if(frameRenderTime >= 1000)
    {
        float fps = frameRenderCount * 1000.0f / frameRenderTime;
        char buffer[256];
        sprintf(buffer, GAME_TITLE " - %03.2f", fps);
        SDL_SetWindowTitle(window, buffer);
        frameRenderCount = 0;
        frameRenderTime = 0;
    }
}

int main(int argc, char* argv[])
{
    SDL_SetHint("SDL_HINT_NO_SIGNAL_HANDLERS", "1");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);
    IMG_Init(IMG_INIT_PNG);

    window = SDL_CreateWindow(GAME_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, screenWidth, screenHeight);

    persistentMemory.reserve(PersistentMemorySize);
    transientMemory.reserve(TransientMemorySize);

    lastUpdateTime = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoopIteration, 60, 1);
#else

    while(!quitting)
    {
        mainLoopIteration();

        int frameDuration = 1000/60;
        int nextFrameTime = lastUpdateTime + frameDuration;
        int delayTime = nextFrameTime - SDL_GetTicks();
        if(delayTime > 0)
            SDL_Delay(delayTime);
    }

    SDL_Quit();
    IMG_Quit();
#endif

    return 0;
}
