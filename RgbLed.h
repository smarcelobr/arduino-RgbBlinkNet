// domain data declaration
typedef struct  {
  unsigned int min_brightness, max_brightness;
  int fadeAmount;
  unsigned int max_delayCount; // millis
} RgbSetup;

struct ColorLed {
  unsigned int pin;
  RgbSetup setup;
  int actual_brightness;
  unsigned int actual_delayCount; // millis
};

#define RED 0
#define GREEN 1
#define BLUE 2
#define NENHUM 3
#define NO_VALUE 9234

ColorLed led[3] = {{3, {1,255, 1, 80},0,0},
                   {5, {1,58, 1, 205},0,0},
                   {9, {1,58, 1, 210},0,0}};
                   

