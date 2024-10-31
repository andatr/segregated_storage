#include <iostream>
#include <thread>
#include <chrono>
#include <Windows.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#include "object_pool/object_pool.h"

// -----------------------------------------------------------------------------------------------------------------------------
void setKeyNonBlocking(bool enable) {
#ifndef _WIN32
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  if (enable) {
    tty.c_lflag &= ~ICANON;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
  }
  else {
    tty.c_lflag |= ICANON;
    fcntl(STDIN_FILENO, F_SETFL, 0);
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
#endif
}

// -----------------------------------------------------------------------------------------------------------------------------
bool keyPoll(char* key) {
#ifdef _WIN32
  if (_kbhit()) {
    *key = _getch();
    return true;
  }
#else
  char c;
  if (read(STDIN_FILENO, &c, 1) > 0) {
    *key = c;
    return true;
  }
#endif
  return false;
}

// -----------------------------------------------------------------------------------------------------------------------------
void setCursorPosition(int x, int y) {
#ifdef _WIN32
    COORD coord = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
#else
    // ANSI escape code to move the cursor
    std::cout << "\033[" << y << ";" << x << "H";
#endif
}

void renderFrame(bool* field1, bool* field2)
{
  setCursorPosition(0, 0);

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {



      if (field2[i * 10 + j]) std::cout << " @ ";
      else std::cout << " . ";

    }
    std::cout << "\n";
  }
  std::cout << std::endl;
}

// -----------------------------------------------------------------------------------------------------------------------------
int main(int, char**)
{
  bool field1[100];
  bool field2[100];

  setKeyNonBlocking(true);
  char key = 0;
  while (true) {
    if (keyPoll(&key)) {
      break;
    }
    renderFrame(field1, field2);
    std::swap(field1, field2);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  setKeyNonBlocking(false);
  return 0;
}