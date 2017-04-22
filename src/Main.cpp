#include "SDL.h"
#include "SDL_main.h"
#include "GameInterface.hpp"

static constexpr size_t PersistentMemorySize = 64*1024*1024;
static constexpr size_t TransientMemorySize = 256*1024*1024;

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

void processEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
        case SDL_KEYDOWN:
            switch(event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                quitting = true;
                break;
            default:
                break;
            }
            break;
        case SDL_QUIT:
            quitting = true;
            break;
        }
    }
}

static void update()
{
}

static void render()
{
    uint8_t *backBuffer;
    int pitch;

    if(currentGameInterface)
    {
        SDL_LockTexture(texture, nullptr, reinterpret_cast<void**> (&backBuffer), &pitch);
        currentGameInterface->render(screenWidth, screenHeight, backBuffer, pitch);
        SDL_UnlockTexture(texture);
    }

    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 0);
    SDL_RenderClear(renderer);
    if(currentGameInterface)
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

static void mainLoopIteration()
{
    reloadGameInterface();
    processEvents();

    update();
    render();
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Test window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, screenWidth, screenHeight);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoopIteration, 60, 1);
#else

    persistentMemory.reserve(PersistentMemorySize);
    transientMemory.reserve(TransientMemorySize);

    while(!quitting)
    {
        mainLoopIteration();
        SDL_Delay(1000/60);
    }

    SDL_Quit();
#endif

    return 0;
}
