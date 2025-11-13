/*
 * Copyright 2021-2022 LVGL LLC
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (1 byte per pixel), 8 (1 byte per pixel), 16 (2 bytes per pixel), 32 (4 bytes per pixel)*/
#define LV_COLOR_DEPTH 16

/*Swap the 2 bytes of RGB565 color. Useful if the display controller requires it.*/
/* Reverted to 0 - swap didn't fix the blue tint issue */
#define LV_COLOR_16_SWAP 0

/*Enable features to draw on transparent background.
 *It's required if opa, stained, etc. screen transformations are used.
 *Can be used with ARGB8888 color format*/
#define LV_COLOR_SCREEN_TRANSP 0

/*Images pixels with this color will not be drawn if they are chroma keyed)*/
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/

/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc` and `lv_mem_free`*/
#define LV_MEM_CUSTOM 0
#if LV_MEM_CUSTOM == 0
/*Size of the memory available for `lv_mem_alloc` in bytes (>= 2kB)*/
#  define LV_MEM_SIZE (48U * 1024U)

/*Set an address for the memory pool instead of allocating it as a static array. Can be in external SRAM too.*/
#  define LV_MEM_ADDR 0
#endif  /*LV_MEM_CUSTOM*/

/*Use the standard `memcpy` and `memset` instead of LVGL's own functions. (Might be faster)*/
#define LV_MEMCPY_MEMSET_STD 0

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh period in milliseconds*/
#define LV_DISP_DEF_REFR_PERIOD 30

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD 30

/*Use a custom tick source that tells the elapsed time in milliseconds.
 *It removes the need to manually update the tick with `lv_tick_inc()`)*/
#define LV_TICK_CUSTOM 0

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/

/*-------------
 * Drawing
 *-----------*/

/*Enable complex draw engine.
 *Required for line, arc, rounded rectangle, paths, adv_rounded rectangle features.*/
#define LV_DRAW_COMPLEX 1

/*Allow rendering to be done in an internal buffer instead of directly to VDB.
 *Typically rendering happens in VDB which is shifted to the display based on the render area.
 *With LV_DRAW_BUF_RENDERING enabled, the rendered image is stored in an internal buffer and LVGL can be
 *configured to transfer it to VDB when the rendering is ready.*/
#define LV_DRAW_BUF_RENDERING 0

/*Set the number of draw units.
 *Draw units are used to parallelize rendering on multiple GPUs.*/
#define LV_DRAW_UNIT_CNT 1

/*Use LVGL's built-in software renderer's functions.
 *Required if no hardware accelerated renderer is available.*/
#define LV_USE_SW_RENDERER 1

/*Default image cache size. Image caching keeps the images opened.
 *If only the built-in image formats are used there is no real advantage of caching. (I.e. if no new image decoder is added)
 *With complex image decoders (e.g. PNG or JPG) caching can save the continuous decoding of images.
 *The value is in bytes.*/
#define LV_IMG_CACHE_DEF_SIZE 0

/*Maximum buffer size to allocate for rotation.
 *Only used if software rotation is enabled in the display driver.*/
#define LV_DISP_ROT_MAX_BUF (10*1024)

/*-------------
 * GPU
 *-----------*/

/*Use Arm's 2D acceleration library Arm-2D*/
#define LV_USE_GPU_ARM2D 0

/*Use STM32's DMA2D (aka Chrom Art) GPU*/
#define LV_USE_GPU_STM32_DMA2D 0

/*Use NXP's PXP GPU i.MX RTxxx platforms*/
#define LV_USE_GPU_NXP_PXP 0

/*Use NXP's VG-Lite GPU i.MX RTxxx platforms*/
#define LV_USE_GPU_NXP_VG_LITE 0

/*Use SWM341's DMA2D GPU*/
#define LV_USE_GPU_SWM341_DMA2D 0

/*-------------
 * Logging
 *-----------*/

/*Enable the log module*/
#define LV_USE_LOG 0

/*-------------
 * Asserts
 *-----------*/

/*Enable asserts if an error is occurred. (Asserts check the validity of the parameters)*/
#define LV_USE_ASSERT_NULL          1   /*Check if the pointers are NULL*/
#define LV_USE_ASSERT_MALLOC        1   /*Check if malloc failed*/
#define LV_USE_ASSERT_STYLE         0   /*Check if the styles are properly initialized*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*Check the integrity of `lv_mem`*/
#define LV_USE_ASSERT_OBJ           0   /*Check the object's type and existence*/

/*Add a custom handler when assert happens e.g. to print a back trace*/
#define LV_ASSERT_HANDLER_INCLUDE <stdint.h>
#define LV_ASSERT_HANDLER while(1);

/*-------------
 * Others
 *-----------*/

/*1: Enable API functions for fonts*/
#define LV_USE_FONT_SUBPX 0

/*1: Enable font compression*/
#define LV_FONT_FMT_TXT_COMPRESSED 0

/*Enable subpixel rendering*/
#define LV_USE_FONT_SUBPX 0
#if LV_USE_FONT_SUBPX
/*Set the pixel order of the display. Physical order of the subpixels.*/
#define LV_FONT_SUBPX_BGR 0  /*0: RGB; 1: BGR order*/
#endif

/*1: add a `user_data` to drivers and objects*/
#define LV_USE_USER_DATA 1

/*1: Show CPU usage and FPS count in the right bottom corner*/
#define LV_USE_PERF_MONITOR 0

/*1: Show the used memory and the memory fragmentation in the left bottom corner
 * Requires LV_MEM_CUSTOM = 0*/
#define LV_USE_MEM_MONITOR 0

/*Change the built-in fonts configuration*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_10 1

/*Declare the built-in fonts*/
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*Enable the advanced typography features*/
#define LV_USE_BIDI 0

/*Enable Arabic/Persian processing*/
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*Enable the file system module*/
#define LV_USE_FS_API 0

/*Enable animations*/
#define LV_USE_ANIMATION 1

/*Enable shadow drawing*/
#define LV_USE_SHADOW 1

/*Enable object groups (for keyboards)*/
#define LV_USE_GROUP 1

/*Enable GPU driver*/
#define LV_USE_GPU 1

/*Enable screenshot*/
#define LV_USE_SNAPSHOT 0

/*Enable Monkey test*/
#define LV_USE_MONKEY 0

/*Enable grid navigation*/
#define LV_USE_GRIDNAV 0

/*Enable IME features*/
#define LV_USE_IME_PINYIN 0

/*Enable FreeType support*/
#define LV_USE_FREETYPE 0

/*Enable QR code generation*/
#define LV_USE_QRCODE 0

/*Enable PNG decoder*/
#define LV_USE_PNG 0

/*Enable BMP decoder*/
#define LV_USE_BMP 0

/*Enable JPG decoder*/
#define LV_USE_SJPG 0

/*Enable GIF decoder*/
#define LV_USE_GIF 0

/*==================
 *    WIDGETS
 *==================*/

/*Arc*/
#define LV_USE_ARC        1

/*Bar*/
#define LV_USE_BAR        1

/*Button*/
#define LV_USE_BTN        1

/*Button matrix*/
#define LV_USE_BTNMATRIX  1

/*Calendar*/
#define LV_USE_CALENDAR   1

/*Canvas*/
#define LV_USE_CANVAS     1

/*Chart*/
#define LV_USE_CHART      1

/*Checkbox*/
#define LV_USE_CHECKBOX   1

/*Color wheel*/
#define LV_USE_COLORWHEEL 1

/*Dropdown*/
#define LV_USE_DROPDOWN   1

/*Image*/
#define LV_USE_IMG        1

/*Image Button*/
#define LV_USE_IMGBTN     1

/*Keyboard*/
#define LV_USE_KEYBOARD   1

/*Label*/
#define LV_USE_LABEL      1
#if LV_USE_LABEL
#  define LV_LABEL_TEXT_SELECTION 1
#  define LV_LABEL_LONG_TXT_HINT 1
#endif

/*LED*/
#define LV_USE_LED        1

/*Line*/
#define LV_USE_LINE       1

/*List*/
#define LV_USE_LIST       1

/*Menu*/
#define LV_USE_MENU       1

/*Message box*/
#define LV_USE_MSGBOX     1

/*Roller*/
#define LV_USE_ROLLER     1

/*Slider*/
#define LV_USE_SLIDER     1

/*Spinbox*/
#define LV_USE_SPINBOX    1

/*Spinner*/
#define LV_USE_SPINNER    1

/*Switch*/
#define LV_USE_SWITCH     1

/*Table*/
#define LV_USE_TABLE      1

/*Tabview*/
#define LV_USE_TABVIEW    1

/*Text area*/
#define LV_USE_TEXTAREA   1

/*Tileview*/
#define LV_USE_TILEVIEW   1

/*Window*/
#define LV_USE_WIN        1

#endif /*LV_CONF_H*/
