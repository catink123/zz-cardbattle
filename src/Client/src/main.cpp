#include <emscripten/emscripten.h>
#include <raylib.h>

const int screenWidth = 800;
const int screenHeight = 450;

int main() {
  InitWindow(screenWidth, screenHeight, "title");

  SetTargetFPS(60);

  BeginDrawing();
  ClearBackground(RAYWHITE);
  DrawText("Hello, raylib!", 190, 200, 20, LIGHTGRAY);
  EndDrawing();

  // CloseWindow();

  return 0;
}
