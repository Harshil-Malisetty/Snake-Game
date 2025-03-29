#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>  // For execl function

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define BUTTON_WIDTH 200
#define BUTTON_HEIGHT 50

typedef enum {
    MENU,
    SINGLE_PLAYER,
    CHALLENGE_MODE,
    TWO_PLAYER,
    QUIT
} GameState;

GameState currentGameState = MENU;

// Button positions
SDL_Rect singlePlayerButton = {300, 200, BUTTON_WIDTH, BUTTON_HEIGHT};
SDL_Rect challengeModeButton = {300, 300, BUTTON_WIDTH, BUTTON_HEIGHT};
SDL_Rect twoPlayerButton = {300, 400, BUTTON_WIDTH, BUTTON_HEIGHT};

// SDL variables
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;

// Function declarations
void renderMenu();
void handleMenuEvents();
void cleanup();
SDL_Texture* renderText(const char* text, SDL_Color color, SDL_Rect* destRect);

// Initialize SDL and TTF
bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    if (TTF_Init() < 0) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }

    window = SDL_CreateWindow("Snake Game",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH, SCREEN_HEIGHT,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }

    // Load font with better error handling
    font = TTF_OpenFont("dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf", 24);
    if (!font) {
        printf("Failed to load font! TTF_Error: %s\n", TTF_GetError());
        printf("Attempted to load font at: dejavu-fonts-ttf-2.37/ttf/DejaVuSans.ttf\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    return true;
}

// Cleanup function
void cleanup() {
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}

// Render text helper function
SDL_Texture* renderText(const char* text, SDL_Color color, SDL_Rect* destRect) {
    if (!font) {
        printf("Error in renderText: Font not loaded!\n");
        return NULL;
    }

    if (!text) {
        printf("Error in renderText: Null text string!\n");
        return NULL;
    }

    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) {
        printf("Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return NULL;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        printf("Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        return NULL;
    }

    destRect->w = surface->w;
    destRect->h = surface->h;
    SDL_FreeSurface(surface);
    return texture;
}

// Render the main menu
void renderMenu() {
    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Color titleColor = {0, 255, 0}; // Green
    SDL_Rect titleRect = {0, 100, 0, 0}; // Width and height will be set by renderText
    SDL_Texture* titleTexture = renderText("Welcome to Snake Game", titleColor, &titleRect);
    // Center the title
    titleRect.x = (SCREEN_WIDTH - titleRect.w) / 2;

    SDL_Color buttonColor = {255, 255, 255}; // White

    // Center text in buttons
    SDL_Rect singlePlayerTextRect = {0, 0, 0, 0};
    SDL_Texture* singlePlayerTexture = renderText("Single Player", buttonColor, &singlePlayerTextRect);
    singlePlayerTextRect.x = singlePlayerButton.x + (singlePlayerButton.w - singlePlayerTextRect.w) / 2;
    singlePlayerTextRect.y = singlePlayerButton.y + (singlePlayerButton.h - singlePlayerTextRect.h) / 2;

    SDL_Rect challengeModeTextRect = {0, 0, 0, 0};
    SDL_Texture* challengeModeTexture = renderText("Challenge Mode", buttonColor, &challengeModeTextRect);
    challengeModeTextRect.x = challengeModeButton.x + (challengeModeButton.w - challengeModeTextRect.w) / 2;
    challengeModeTextRect.y = challengeModeButton.y + (challengeModeButton.h - challengeModeTextRect.h) / 2;

    SDL_Rect twoPlayerTextRect = {0, 0, 0, 0};
    SDL_Texture* twoPlayerTexture = renderText("2 Player", buttonColor, &twoPlayerTextRect);
    twoPlayerTextRect.x = twoPlayerButton.x + (twoPlayerButton.w - twoPlayerTextRect.w) / 2;
    twoPlayerTextRect.y = twoPlayerButton.y + (twoPlayerButton.h - twoPlayerTextRect.h) / 2;

    // Draw title
    SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);

    // Draw buttons
    SDL_SetRenderDrawColor(renderer, 50, 50, 150, 255); // Dark blue button
    SDL_RenderFillRect(renderer, &singlePlayerButton);
    SDL_RenderFillRect(renderer, &challengeModeButton);
    SDL_RenderFillRect(renderer, &twoPlayerButton);

    // Draw button borders
    SDL_SetRenderDrawColor(renderer, 80, 80, 200, 255); // Light blue border
    SDL_RenderDrawRect(renderer, &singlePlayerButton);
    SDL_RenderDrawRect(renderer, &challengeModeButton);
    SDL_RenderDrawRect(renderer, &twoPlayerButton);

    // Draw button texts
    SDL_RenderCopy(renderer, singlePlayerTexture, NULL, &singlePlayerTextRect);
    SDL_RenderCopy(renderer, challengeModeTexture, NULL, &challengeModeTextRect);
    SDL_RenderCopy(renderer, twoPlayerTexture, NULL, &twoPlayerTextRect);

    // Present renderer
    SDL_RenderPresent(renderer);

    // Clean up textures
    SDL_DestroyTexture(titleTexture);
    SDL_DestroyTexture(singlePlayerTexture);
    SDL_DestroyTexture(challengeModeTexture);
    SDL_DestroyTexture(twoPlayerTexture);
}

// Function to launch another program
void launchProgram(const char* programPath) {
    cleanup(); // Clean up SDL resources before executing new program

    // Print debug info
    printf("Attempting to launch: %s\n", programPath);

    // Execute the program (replace the current process)
    execl(programPath, programPath, NULL);

    // If execl returns, it failed
    perror("execl failed");
    exit(EXIT_FAILURE);
}

// Handle menu clicks
void handleMenuEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            currentGameState = QUIT;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            int x = event.button.x;
            int y = event.button.y;

            // Debug print to see where clicks are being registered
            printf("Mouse click at x=%d, y=%d\n", x, y);

            if (x >= singlePlayerButton.x && x < singlePlayerButton.x + singlePlayerButton.w &&
                y >= singlePlayerButton.y && y < singlePlayerButton.y + singlePlayerButton.h) {
                printf("Single Player button clicked!\n");
                launchProgram("./attempt");
            } else if (x >= challengeModeButton.x && x < challengeModeButton.x + challengeModeButton.w &&
                      y >= challengeModeButton.y && y < challengeModeButton.y + challengeModeButton.h) {
                printf("Challenge Mode button clicked!\n");
                launchProgram("./challenge");
            } else if (x >= twoPlayerButton.x && x < twoPlayerButton.x + twoPlayerButton.w &&
                      y >= twoPlayerButton.y && y < twoPlayerButton.y + twoPlayerButton.h) {
                printf("Two Player button clicked!\n");
                launchProgram("./multiplayer");
            }
        }
    }
}

// Main loop
int main(int argc, char* argv[]) {
    if (!init()) {
        return 1;
    }

    bool running = true;
    while (running) {
        switch (currentGameState) {
            case MENU:
                renderMenu();
                handleMenuEvents();
                break;
            case QUIT:
                running = false;
                break;
            default:
                // This should not happen with the new implementation
                printf("Unexpected game state!\n");
                running = false;
                break;
        }

        // Small delay to prevent CPU hogging in the main loop
        SDL_Delay(16); // ~60 FPS
    }

    cleanup();
    return 0;
}
