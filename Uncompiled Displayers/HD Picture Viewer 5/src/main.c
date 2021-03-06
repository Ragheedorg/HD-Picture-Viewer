#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fileioc.h>
#include <debug.h>

/* globals */

#define BYTES_PER_IMAGE_NAME 9 //8 for image name, 1 for null terminator
#define MAX_IMAGES 936 //Max images is this because max combinations of appvars goes up to that
#define TASKS_TO_FINISH 2
#define X_MARGIN 8
#define Y_MARGIN 38
#define Y_SPACING 25
#define MAX_THUMBNAIL_WIDTH 160
#define MAX_THUMBNAIL_HEIGHT 240
#define THUMBNAIL_ZOOM 0




/* Function Prototyptes */
void combineSquareID(char *squareName, uint24_t x, uint24_t y, char *id);
uint8_t databaseReady();
void DisplayHomeScreen(uint24_t pics);
void DrawImage(uint24_t picName);
void noImagesFound();
void PrintCentered(const char *str);
void PrintCenteredX(const char *str, uint8_t y);
void PrintCenteredY(const char *str, uint8_t x);
void printNames(uint24_t start, char *picNames, uint24_t numOfPics);
void printText(int8_t xpos, int8_t ypos, const char *text);
uint24_t rebuildDB(uint8_t p);
void SplashScreen();
void SetLoadingBarProgress(uint8_t p, uint8_t t);

/* Main function, called first */
int main(void)
{
  uint8_t ready=0, tasksFinished = 0;
  uint24_t picsDetected=0;
  /* Clear the homescreen */
  //os_ClrHome();

  gfx_Begin();
  ti_CloseAll();
  SplashScreen();
  SetLoadingBarProgress(++tasksFinished, TASKS_TO_FINISH);
  //checks if the database exists and is ready 0 failure; 1 created; 2 exists
  ready = databaseReady();
  if (ready==0)
  goto err;


  picsDetected=rebuildDB(tasksFinished);
  if(picsDetected==0)
  goto err;
  //returns how many images were found. 0 means quit.

  SetLoadingBarProgress(++tasksFinished,TASKS_TO_FINISH);
  DisplayHomeScreen(picsDetected);

  err:
  /* Waits for a keypress */
  while (!os_GetCSC());
  ti_CloseAll();
  gfx_End();

  /* Return 0 for success */
  return 0;

}

/* Functions */

void DisplayHomeScreen(uint24_t pics){
  char *picNames = malloc(pics*BYTES_PER_IMAGE_NAME); //BYTES_PER_IMAGE_NAME = 9
  ti_var_t database = ti_Open("HDPICDB","r");
  uint24_t i,startName=0;
  uint8_t Ypos=10;
  kb_key_t key = kb_Data[7];
  bool up,down,left,right;


  //makes the screen black and sets text white
  gfx_FillScreen(0);
  gfx_SetTextFGColor(254);
  gfx_SetTextBGColor(0);
  gfx_SetColor(255);
  gfx_VertLine(140,20,200);


  //seeks to the first image name
  ti_Seek(8,SEEK_SET,database);
  //PrintCenteredX("Test1",30);
  //loops through every picture that was detected and store the image name to picNames
  for(i=0;i<=pics;i++){
    ti_Read(&picNames[i * BYTES_PER_IMAGE_NAME],8,1,database);
    picNames[i * BYTES_PER_IMAGE_NAME + BYTES_PER_IMAGE_NAME - 1] = 0;
    ti_Seek(8,SEEK_CUR,database);
    //PrintCenteredX("Test2",40);
    Ypos+=15;
    //PrintCenteredX(&picNames[i*BYTES_PER_IMAGE_NAME],Ypos);

  }

  /* Keypress handler */
  gfx_SetTextXY(10,10);
  printNames(startName, picNames, pics);
  DrawImage(startName);
  do{
    //scans the keys for keypress
    kb_Scan();
    //checks if up or down arrow key were pressed
    key = kb_Data[7];
    down= key & kb_Down;
    up  = key & kb_Up;
    //if any key was pressed
    if(key){
      /* increases the name to start on and redraws the text */
      if(down){
        //PrintCenteredX("DOWN",10);
        startName++;
        //make sure user can't scroll down too far
        if (startName>(pics-1))//If there's more than 4 images, then handle things normally
        startName=pics-1;
        if (startName>pics-1 && pics-1>0) //makes sure user can't scroll too far when there's only 1 image detected
        startName=pics;
        if (startName>pics-2 && pics-2>0) //makes sure user can't scroll too far when there's only 2 images detected
        startName=pics-1;
        if (startName>pics-3 && pics-3>0) //makes sure user can't scroll too far when there's only 3 images detected
        startName=pics-2;
        printNames(startName, picNames, pics);
        DrawImage(startName);
      }
      //key = kb_Data[3];

      /* decreases the name to start on and redraws the text */
      if(up){
        //PrintCenteredX(" UP ",10);
        startName--;
        /*checks if startName underflowed from 0 to 16 million or something.
        * Whatever the number, it shouldn't be less than the max number of images possible*/
        if (startName>MAX_IMAGES)
        startName=0;
        printNames(startName, picNames, pics);
        DrawImage(startName);
      }
    }


  }   while(kb_Data[6]!=kb_Clear);

  free(picNames);
}

/* Draws the image stored in database at position startName.
* Draws the image at location x,y starting at top left corner.
* If x=-1 then make image horizontally centered in the screen.
* If y=-1 then make image vertically centered on the screen.
* Image will automatically be resized to same aspect ratio so you just set the max width and height (4,3 will fit the screen normally)
*
*/
void DrawImage(uint24_t picName){
  ti_var_t database = ti_Open("HDPICDB","r"),squareSlot,palSlot;
  char imgWH[6], imgID[2], searchName[9], palName[9];
  uint24_t i=0,widthSquares=0,heightSquares=0,widthPx=0,heightPx=0,xSquare=0,ySquare=0,xOffsetSquare=0,yOffsetSquare=0;
  uint16_t *palPtr[256];
  double scale;
  gfx_sprite_t *squareImg;
  gfx_FillScreen(0);

  //seeks past header (8bytes), imgName, and unselected images
  ti_Seek(16+(16*picName),SEEK_CUR,database);
  //reads the image letter ID (2 bytes)
  ti_Read(imgID,2,1,database);
  //reads the image width/height (6 bytes)
  ti_Read(imgWH,6,1,database);
  //closes database
  ti_Close(database);

  //Converts the width/height from a char array into two integers by converting char into decimal value
  //then subtracting 48 to get the actuall number.
  gfx_SetTextScale(1,1);
  //gfx_PrintStringXY(imgWH,170,10);
  dbg_sprintf(dbgout, imgWH);


//converts the char numbers into uint numbers
  widthSquares=(uint24_t)imgWH[0]-48+(uint24_t)imgWH[1]-48+(uint24_t)imgWH[2]-48;
  heightSquares=(uint24_t)imgWH[3]-48+(uint24_t)imgWH[4]-48+(uint24_t)imgWH[5]-48;
  //will not work unless this is here

  /*
  dbg_sprintf(dbgout, "WS value: %d\n", widthSquares);
  dbg_sprintf(dbgout, "HS value: %d\n", heightSquares);
  gfx_PrintUInt(widthSquares,3);
  gfx_PrintUInt(heightSquares,3);
  gfx_SetTextScale(1,1);
  gfx_PrintStringXY("WS",170,60);
  gfx_SetTextXY(200,60);
  gfx_PrintUInt(widthSquares,3);
  gfx_PrintStringXY("HS",170,70);
  gfx_SetTextXY(200,70);
  gfx_PrintUInt(heightSquares,3);*/

  //makes sure image will fit on screen according to the max width/height provided in the requirements
  /*if(widthSquares>(mWidth/80){
  scale=4.0/(double)widthSquares;
  if(heightSquares*scale>(mHeight/80)){
  scale = 3.0/(double)heightSquares;
}
}else if(heightSquares>3){
scale = 3.0/(double)heightSquares;
}
if (x<0){
xOffsetSquare = (double)x/80
}*/

//sets correct palettes the
sprintf(palName, "HP%.2s0000\0",imgID);
palSlot = ti_Open(palName,"r");
if (!palSlot){
  PrintCenteredX(palName,120);
  PrintCenteredX(" Palette does not exist!",130);
  while(!os_GetCSC());
  ti_CloseAll();
  return;
}

//gfx_SetDrawBuffer();
ti_Seek(24,SEEK_SET,palSlot);
squareImg = ti_GetDataPtr(palSlot);
gfx_SetPalette(squareImg,512,0);
ti_Close(palSlot);
//Will not work without this here

/*
dbg_sprintf(dbgout, "WS value: %d\n", widthSquares);
dbg_sprintf(dbgout, "HS value: %d\n", heightSquares);
gfx_PrintUInt(widthSquares,3);
gfx_PrintUInt(heightSquares,3);

gfx_FillScreen(0);
gfx_SetTextScale(1,1);
gfx_PrintStringXY("WS",170,60);
gfx_SetTextXY(200,60);

gfx_PrintStringXY("HS",170,70);
gfx_SetTextXY(200,70);
gfx_PrintUInt(heightSquares,3);*/


for(xSquare=0;xSquare<(widthSquares+1);xSquare++){
  for(ySquare=0;ySquare<(heightSquares+1);ySquare++){
    sprintf(searchName, "%.2s%03u%03u\0",imgID, xSquare, ySquare);//combines the separate parts into one name to search for
    /*gfx_PrintStringXY("XS",170,80);
    gfx_SetTextXY(200,80);
    gfx_PrintUInt(xSquare,3);
    gfx_PrintStringXY("YS",170,90);
    gfx_SetTextXY(200,90);
    gfx_PrintUInt(ySquare,3);
    gfx_PrintStringXY(searchName,200,100+10*ySquare);*/
    //while(!os_GetCSC());
    //squareSlot = ti_Open(searchName,"r");
    /*This opens the variable with the name that was just assembled.
    * It then gets the pointer to that and stores it in a graphics variable
    */
    squareSlot = ti_Open(searchName,"r");
    if (!squareSlot){
      PrintCenteredX(searchName,120);
      PrintCentered("Square doesn't exist!");
      while(!os_GetCSC());
      ti_CloseAll();
      return;
    }
    ti_Seek(16,SEEK_CUR,squareSlot);
    squareImg = (gfx_sprite_t*)ti_GetDataPtr(squareSlot);
    //displays the image
    gfx_ScaledSprite_NoClip(squareImg,xSquare*80,ySquare*80,1,1);

    ti_Close(squareSlot);
    //[commandz] you'll want to either ti_Read the sprite out,
    //or just open the file and use gfx_sprite_t *sprite = (gfx_sprite_t*)ti_GetDataPtr(slot);
  }
}
//gfx_SwapDraw();
//ti_Close(palSlot);
}

void combineSquareID(char *squareName, uint24_t x, uint24_t y, char *id){
  squareName[0]=x/100;
  squareName[1]=(x/10)-((x/100)*100);
  squareName[2]=x-((x/10)*10)-((x/100)*100);
  squareName[3]=y/100;
  squareName[4]=(y/10)-((y/100)*100);
  squareName[5]=y-((y/10)*10)-((y/100)*100);
  squareName[6]=id[0];
  squareName[7]=id[1];
  squareName[8]='\0';
  //return squareName;
}

/* This UI keeps the user selection in the middle of the screen. */
void printNames(uint24_t startName, char *picNames, uint24_t numOfPics){
  uint24_t i, Yoffset=0, y=0, curName=0;

  //clears old text and sets up for new text
  gfx_SetTextScale(2,2);
  gfx_SetColor(0);
  gfx_FillRectangle_NoClip(0,0,140,240);
  gfx_SetColor(255);

  //re-draws UI lines
  gfx_HorizLine_NoClip(0,120,6);
  gfx_HorizLine_NoClip(136,120,5);
  gfx_HorizLine_NoClip(6,110,130);
  gfx_HorizLine_NoClip(6,130,130);
  gfx_VertLine_NoClip(6,110,20);
  gfx_VertLine_NoClip(136,110,21);

  /*if the selected start name is under 4, that means we need to start drawing
  * farther down the screen for the text to go in the right spot */
  gfx_SetTextXY(200,30);
  gfx_PrintUInt(startName,3);
  if(startName<4){
    Yoffset = 75 - startName * Y_SPACING;
    startName = 0;
  }else{
    startName-=4;
  }
  curName=startName;


  /* draw the text on the screen. Starts displaying the name at element start
  * then iterates until out of pics or about to draw off the screen */
  for(i=0;i<numOfPics && y<180;i++){
    //calculates where the text should be drawn
    y = i * Y_SPACING + Y_MARGIN + Yoffset;

    /*
    // debug stuff
    gfx_SetTextScale(1,1);
    gfx_SetTextXY(200,10);
    gfx_PrintUInt(i,3);
    gfx_SetTextXY(200,20);
    gfx_PrintUInt(y,3);
    gfx_SetTextXY(200,40);
    gfx_PrintUInt(curName,3);
    gfx_SetTextXY(200,50);
    gfx_PrintUInt(numOfPics,3);
    gfx_SetTextScale(2,2);
    */
    //Prints out the correct name
    gfx_PrintStringXY(&picNames[curName++*BYTES_PER_IMAGE_NAME],X_MARGIN,y);
    //while(!os_GetCSC());

  }
  //slows down scrolling speed
  delay(150);
}



/* Rebuilds the database of images on the calculator*/
uint24_t rebuildDB(uint8_t p){
  char *var_name, *imgInfo[16], nameBuffer[10];
  uint8_t *search_pos = NULL;
  uint24_t imagesFound=0;
  char myData[8]="HDPALV1",names[8];
  ti_var_t database = ti_Open("HDPICDB","w"), palette;
  ti_Write("HDDATV10",8,1,database);//Rewrites the header because w overwrites everything

  //resets splash screen for new loading SetLoadingBarProgress
  SplashScreen();

  /*
  * Searches for palettes. This is a lot easier than searching for every single
  * image square because there's is guarunteed to only be one palette per image.
  * The palette containts all the useful information such as the image size and
  * the two letter ID for each appvar. This makes it easy to find every square via a loop.
  */
  while((var_name = ti_DetectVar(&search_pos, "HDPALV10", TI_APPVAR_TYPE)) != NULL) {
    //sets progress of how many images were found
    SetLoadingBarProgress(++imagesFound,MAX_IMAGES);
    //finds the name, letter ID, and size of entire image this palette belongs to.
    palette = ti_Open(var_name,"r");
    //seeks past useless info
    ti_Seek(8,SEEK_CUR,palette);
    ti_Seek(16,SEEK_CUR,database);
    //reads the important info (16 bytes)
    ti_Read(&imgInfo,16,1,palette);
    //Writes the info to the database
    ti_Write(imgInfo,16,1,database);
    //closes palette for next iteration
    ti_Close(palette);
  }
  //closes the database
  ti_Close(database);
  gfx_End();
  ti_SetArchiveStatus(true,database);
  gfx_Begin();
  SplashScreen();
  gfx_SetTextXY(100,195);
  gfx_PrintUInt(imagesFound,3);
  if (imagesFound==0){
    noImagesFound();
  }
  SetLoadingBarProgress(++p,TASKS_TO_FINISH);



  return imagesFound;
}

void noImagesFound(){
  gfx_SetTextFGColor(192);
  PrintCenteredX("No Pictures Detected!",1);
  gfx_SetTextFGColor(0);
  PrintCenteredX("Convert some images and send them to your",11);
  PrintCenteredX("calculator using the HDpic converter!",21);
  PrintCenteredX("Tutorial:  https://youtu.be/s1-g8oSueQg",31);
  PrintCenteredX("Press any key to quit",41);
  return;
}

//checks if the database is already created. If not, it creates it.
uint8_t databaseReady(){
  char *var_name;
  uint8_t *search_pos = NULL, exists=0, ready = 0;
  ti_var_t myAppVar;
  char myData[9]="HDDATV10"; //remember have one more space than text you're saving
  char compare[9]="HDDATV10";
  //tries to find database using known header
  while((var_name = ti_DetectVar(&search_pos, myData, TI_APPVAR_TYPE)) != NULL) {
    exists=1;
  }
  //if file already exists, simply return
  if (exists == 1)
  ready = 2;
  else{
    //if file doesn't already exist, create it.
    //creates the database appvar and writes the header. Checks if wrote successfuly
    myAppVar=ti_Open("HDPICDB", "w");
    if(!myAppVar)
    ready = 3;
    if(ti_Write(&myData,8,1,myAppVar)!=1)
    ready = 4;
    if (ti_Rewind(myAppVar) == EOF)
    ready = 5;
    if (ti_Read(&myData,8, 1, myAppVar) != 1)
    ready = 6;
    if (strcmp(myData,compare)!=0)
    ready = 7;
    else{
      ready = 1;
    }
  }
  ti_CloseAll();

  //checks what happened
  if(ready==1){
    gfx_SetTextFGColor(195);
    PrintCenteredX("created",180);
    return 1;
  }else if(ready==2){
    gfx_SetTextFGColor(004);
    PrintCenteredX("exists",180);
    return 2;
  }else{
    gfx_SetTextFGColor(224);
    PrintCenteredX("failure",180);
    gfx_SetTextXY(120,190);
    gfx_PrintUInt(ready,1);
    return 0;
  }


}

//makes a loading bar and fills it in depending on progress made (p) / tasks left (t)
void SetLoadingBarProgress(uint8_t p, uint8_t t){
  p=((double)p/(double)t)*200.0;
  //ensures loading bar doesn't go past max point
  if (p>200)
  p=200;

  gfx_SetColor(128);
  gfx_FillRectangle_NoClip(60,153,(uint8_t)p,7);

}

//creates a simple splash screen when program starts
void SplashScreen(){
  //sets color to grey
  gfx_SetColor(181);
  gfx_FillRectangle_NoClip(60,80,LCD_WIDTH-120,LCD_HEIGHT-160);
  /* Print a string */
  PrintCentered("HD Picture Viewer");
}

/* Prints a screen centered string */
void PrintCentered(const char *str)
{
  gfx_PrintStringXY(str,(LCD_WIDTH - gfx_GetStringWidth(str)) / 2, (LCD_HEIGHT - 8) / 2);
}
/* Prints a X centered string */
void PrintCenteredX(const char *str, uint8_t y)
{
  gfx_PrintStringXY(str, (LCD_WIDTH - gfx_GetStringWidth(str)) / 2, y);
}
/* Prints a Y centered string */
void PrintCenteredY(const char *str, uint8_t x)
{
  gfx_PrintStringXY(str, x, (LCD_HEIGHT - 8) / 2);
}


/* Draw text on the homescreen at the given X/Y location */
void printText(int8_t xpos, int8_t ypos, const char *text) {
  os_SetCursorPos(ypos, xpos);
  os_PutStrFull(text);
}
