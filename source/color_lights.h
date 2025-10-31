
#ifndef COLOR_LIGHTS_H_
#define COLOR_LIGHTS_H_

#include "aqualink.h"
#include "aq_programmer.h"

#define LIGHT_COLOR_NAME    16
#define LIGHT_COLOR_OPTIONS 19
//#define LIGHT_DIMER_OPTIONS 4
//#define LIGHT_COLOR_TYPES   LC_DIMMER+1

// The status returned from RS Serial Adapter has this added as a base.  
#define RSSD_COLOR_LIGHT_OFFSET  64
#define RSSD_DIMMER_LIGHT_OFFSET 128

//#define DIMMER_LIGHT_TYPE_INDEX 10

/*
// color light modes (Aqualink program, Jandy, Jandy LED, SAm/SAL, Color Logic, Intellibrite)
typedef enum clight_type {
  LC_PROGRAMABLE=0, 
  LC_JANDY, 
  LC_JANDYLED, 
  LC_SAL, 
  LC_CLOGIG, 
  LC_INTELLIB
} clight_type;
*/
//const char *light_mode_name(clight_type type, int index);
const char *get_currentlight_mode_name(clight_detail light, emulation_type protocol);
const char *light_mode_name(clight_type type, int index, emulation_type protocol);
int build_color_lights_js(struct aqualinkdata *aqdata, char* buffer, int size);
int build_color_light_jsonarray(int index, char* buffer, int size);

void clear_aqualinkd_light_modes();
bool set_currentlight_value(clight_detail *light, int index);

bool set_aqualinkd_light_mode_name(char *name, int index, bool isShow);
const char *get_aqualinkd_light_mode_name(int index, bool *isShow);
int get_currentlight_mode_name_count(clight_detail light, emulation_type protocol);

int get_num_light_modes(int index);

//char *_color_light_options_[LIGHT_COLOR_TYPES][LIGHT_COLOR_OPTIONS][LIGHT_COLOR_NAME];

#endif //COLOR_LIGHTS_H_
/*

Rev T.2 has the below
Jandy colors    <- Same
Jandy LED       <- Same
SAm/Sal         <- Same
IntelliBrite    <- Same
Hayw Univ Col   <- Color Logic

*/
/*
Color Name      Jandy Colors    Jandy LED       SAm/SAL         Color Logic     IntelliBrite      dimmer
---------------------------------------------------------------------------------------------------------
Color Splash    11                              8                                              
Alpine White    1               1                                                              
Sky Blue        2               2                                                              
Cobalt Blue     3               3                                                              
Caribbean Blu   4               4                                                              
Spring Green    5               5                                                              
Emerald Green   6               6                                                              
Emerald Rose    7               7                                                              
Magenta         8               8                                                              
Garnet Red      9               9                                                              
Violet          10              10                                                             
Slow Splash                     11                                                             
Fast Splash                     12                                                             
USA!!!                          13                                                             
Fat Tuesday                     14                                                             
Disco Tech                      15                                                             
White                                           1                                              
Light Green                                     2                                              
Green                                           3                                              
Cyan                                            4                                              
Blue                                            5                                              
Lavender                                        6                                              
Magenta                                         7                                              
Light Magenta                                                                                  
Voodoo Lounge                                                   1                              
Deep Blue Sea                                                   2                              
Afternoon Skies                                                 3                              
Afternoon Sky                                                                                 
Emerald                                                         4                              
Sangria                                                         5                              
Cloud White                                                     6                              
Twilight                                                        7                              
Tranquility                                                     8                              
Gemstone                                                        9                             
USA!                                                            10                             
Mardi Gras                                                      11                             
Cool Cabaret                                                    12  
SAm                                                                             1              
Party                                                                           2              
Romance                                                                         3              
Caribbean                                                                       4              
American                                                                        5              
Cal Sunset                                                                      6              
Royal                                                                           7              
Blue                                                                            8              
Green                                                                           9              
Red                                                                             10             
White                                                                           11             
Magenta                                                                         12 
25%                                                                                             1
50%                                                                                             2
75%                                                                                             3
100%                                                                                            4
*/
