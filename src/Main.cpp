#include "SDL.h"
#include "SDL_main.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int screenWidth = 640;
static int screenHeight = 480;
static bool quitting = false;
SDL_Window *window;
SDL_Renderer *renderer;

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

void render()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void mainLoopIteration()
{
    processEvents();
    render();
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Test window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_PRESENTVSYNC);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoopIteration, 60, 1);
#else
    while(!quitting)
    {
        mainLoopIteration();
        SDL_Delay(1000/60);
    }

    SDL_Quit();
#endif

    return 0;
}
