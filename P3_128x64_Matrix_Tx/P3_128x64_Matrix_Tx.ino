/*
// - Lien vidéo: https://youtu.be/xCAm8l4f6Mk
//   REQUIRES the following Arduino libraries:
// - SmartMatrix Library: https://github.com/pixelmatix/SmartMatrix
// - Adafruit_GFX Library: https://github.com/adafruit/Adafruit-GFX-Library
// - AnimatedGIF Library:  https://github.com/bitbank2/AnimatedGIF
// Getting Started ESP32 Px Matrix With SmartMatrix:  https://youtu.be/InhCc_-RBb4
*/

#define USE_ADAFRUIT_GFX_LAYERS
#include <MatrixHardware_ESP32_V0.h>                // This file contains multiple ESP32 hardware configurations, edit the file to define GPIOPINOUT
#include <SmartMatrix.h>
#include <SD.h>
#include <AnimatedGIF.h>

#define COLOR_DEPTH 24                  // Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is good for most sketches - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24)
const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 64;      // Set to the height of your display
const uint8_t kRefreshDepth = 24;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_64ROW_MOD32SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);


#define UpHeader 0x9C
#define endHeader 0x36 

#define MATRIX_WIDTH 128
Stream* mySeriel;
AnimatedGIF gif;
File f;
long lastTime;
const uint16_t NUM_LEDS = kMatrixWidth * kMatrixHeight;
rgb16 usPalette[255];
uint8_t buff[NUM_LEDS];
uint8_t buff1[NUM_LEDS];

IRAM_ATTR void IRQ_HANDLER(void *);
void updateScreenCallback(void);

void setDriver(Stream* s) {
  mySeriel = s;
}

void updateScreenCallback(void) {
  uint8_t c;
  rgb24 *buffer = backgroundLayer.backBuffer();
  for (int i=0; i<NUM_LEDS; i++) {
    c = buff1[i];
    if (c != 0xFF) {
      buffer[i] = usPalette[c];
    }
  }
  backgroundLayer.swapBuffers();
}

// Draw a line of image directly on the LED Matrix
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t usTemp[128];
  int x = pDraw->iX,y = (pDraw->iY + pDraw->y), xy; 
  if (pDraw->y ==0){
    memset(buff, 0xFF, NUM_LEDS);
    memset(buff1, 0xFF, NUM_LEDS);
    memcpy(usPalette, (uint8_t *)pDraw->pPalette, 510);
  }
  memset(usTemp, 0xFF, 128);
  memcpy((usTemp+x), (uint8_t *)pDraw->pPixels, pDraw->iWidth);
  xy = kMatrixWidth * y;
  memcpy((buff+xy), usTemp, kMatrixWidth);
  memcpy((buff1+xy), (usTemp+kMatrixWidth), kMatrixWidth);
} /* GIFDraw() */


void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  //Serial.print("Playing gif: ");
  //Serial.println(fname);
  f = SD.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

void ShowGIF(char *name)
{
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    while (gif.playFrame(true, NULL))
    { 
      mySeriel->write(UpHeader);
      mySeriel->write((uint8_t *)usPalette, 255*2);
      mySeriel->write((uint8_t *)buff, NUM_LEDS);
      mySeriel->write(endHeader);
      updateScreenCallback();
    }
    gif.close();
  }

} /* ShowGIF() */

void setup() {
  Serial.begin(1300000);
  setDriver(&Serial);
  delay(5000);
  matrix.addLayer(&backgroundLayer); 
  matrix.begin();
  backgroundLayer.setBrightness(255);
  //backgroundLayer.enableColorCorrection(true);
  backgroundLayer.setFont(font3x5);
  SD.begin(3);
  gif.begin(LITTLE_ENDIAN_PIXELS);
}

String gifDir = "/gifs"; // play all GIFs in this directory on the SD card
char filePath[256] = { 0 };
File root, gifFile;

void loop() 
{  
      root = SD.open(gifDir);
      if (root) {
        gifFile = root.openNextFile();
        while (gifFile) {
          memset(filePath, 0x0, sizeof(filePath));                
          strcpy(filePath, gifFile.name());
          ShowGIF(filePath);
          gifFile.close();
          gifFile = root.openNextFile();
          }
         lastTime = millis();
         root.close();
      } // root
  if (millis() - lastTime > 3000) {
      backgroundLayer.fillScreen({ 0, 0, 0 });
      backgroundLayer.drawString(3, 24, { 255, 0, 255 }, "Waiting");
      backgroundLayer.swapBuffers();
      lastTime = millis();
   }
      
      delay(4000); // pause before restarting
      
}
