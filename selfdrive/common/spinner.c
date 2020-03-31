#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "nanovg.h"
#define NANOVG_GLES3_IMPLEMENTATION
#include "nanovg_gl.h"
#include "nanovg_gl_utils.h"

#include "framebuffer.h"
#include "spinner.h"

#define SPINTEXT_LENGTH 192

// external resources linked in
extern const unsigned char _binary_opensans_semibold_ttf_start[];
extern const unsigned char _binary_opensans_semibold_ttf_end[];

extern const unsigned char _binary_img_spinner_track_png_start[];
extern const unsigned char _binary_img_spinner_track_png_end[];

extern const unsigned char _binary_img_spinner_comma_png_start[];
extern const unsigned char _binary_img_spinner_comma_png_end[];

bool stdin_input_available() {
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds);
  select(STDIN_FILENO+1, &fds, NULL, NULL, &timeout);
  return (FD_ISSET(0, &fds));
}

int spin(int argc, char** argv) {
  int err;

  bool draw_progress = false;
  bool has_extra = false;
  bool err_msg = false;
  float progress_val = 0.0;

  char *spinstatus;
  char *spinerr;
  char spintext[SPINTEXT_LENGTH];
  spintext[0] = 0;

  const char* spintext_arg = NULL;
  if (argc >= 2) {
    strncpy(spintext, argv[1], SPINTEXT_LENGTH);
  }

  // spinner
  int fb_w, fb_h;
  FramebufferState *fb = framebuffer_init("spinner", 0x00001000, false,
                                          &fb_w, &fb_h);
  assert(fb);

  NVGcontext *vg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
  assert(vg);

  int font = nvgCreateFontMem(vg, "Bold", (unsigned char*)_binary_opensans_semibold_ttf_start, _binary_opensans_semibold_ttf_end-_binary_opensans_semibold_ttf_start, 0);
  assert(font >= 0);

  int spinner_img = nvgCreateImageMem(vg, 0, (unsigned char*)_binary_img_spinner_track_png_start, _binary_img_spinner_track_png_end - _binary_img_spinner_track_png_start);
  assert(spinner_img >= 0);
  int spinner_img_s = 360;
  int spinner_img_x = ((fb_w/2)-(spinner_img_s/2));
  int spinner_img_y = 260;
  int spinner_img_xc = (fb_w/2);
  int spinner_img_yc = (fb_h/2)-100;
  int spinner_comma_img = nvgCreateImageMem(vg, 0, (unsigned char*)_binary_img_spinner_comma_png_start, _binary_img_spinner_comma_png_end - _binary_img_spinner_comma_png_start);
  assert(spinner_comma_img >= 0);

  for (int cnt = 0; ; cnt++) {
    // Check stdin for new text
    if (stdin_input_available()){
      fgets(spintext, SPINTEXT_LENGTH, stdin);

      printf("spintext 1: %s\n", spintext);
      for (int i = 0; i < strlen(spintext); i++) {
        if (spintext[i] == '\x1f') {  // unit separator
          printf("before: %cend\n", spintext[i]);
          spintext[i] = '\n';
          printf("after: %cend\n", spintext[i]);
        }
      }
      printf("spintext 2: %s\n", spintext);
      if (spintext[strlen(spintext)] == '\0' && spintext[strlen(spintext) - 1] == '\n') {
        spintext[strlen(spintext) - 1] = 0;
      }
      printf("spintext 3: %s\n", spintext);
      // Get current status
      has_extra = strchr(spintext, ',') != NULL;
      if (has_extra) {
        spinstatus = strchr(spintext, ',');  // split spintext and error message
        *spinstatus++ = '\0';
        err_msg = strstr(spinstatus, "ERR,") != NULL;
        if (err_msg) {
          spinerr = spinstatus + 4;
        }
      }

      // Check if number (update progress bar)
      size_t len = strlen(spintext);
      bool is_number = len > 0;
      for (int i = 0; i < len; i++){
        if (!isdigit(spintext[i])){
          is_number = false;
          break;
        }
      }

      if (is_number) {
        progress_val = (float)(atoi(spintext)) / 100.0;
        progress_val = fmin(1.0, progress_val);
        progress_val = fmax(0.0, progress_val);
      }

      draw_progress = is_number;
    }

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    nvgBeginFrame(vg, fb_w, fb_h, 1.0f);

    // background
    nvgBeginPath(vg);
    NVGpaint bg = nvgLinearGradient(vg, fb_w, 0, fb_w, fb_h,
    nvgRGBA(0, 0, 0, 175), nvgRGBA(0, 0, 0, 255));
    nvgFillPaint(vg, bg);
    nvgRect(vg, 0, 0, fb_w, fb_h);
    nvgFill(vg);

    // spin track
    nvgSave(vg);
    nvgTranslate(vg, spinner_img_xc, spinner_img_yc);
    nvgRotate(vg, (3.75*M_PI * cnt/120.0));
    nvgTranslate(vg, -spinner_img_xc, -spinner_img_yc);
    NVGpaint spinner_imgPaint = nvgImagePattern(vg, spinner_img_x, spinner_img_y,
      spinner_img_s, spinner_img_s, 0, spinner_img, 0.6f);
    nvgBeginPath(vg);
    nvgFillPaint(vg, spinner_imgPaint);
    nvgRect(vg, spinner_img_x, spinner_img_y, spinner_img_s, spinner_img_s);
    nvgFill(vg);
    nvgRestore(vg);

    // comma
    NVGpaint comma_imgPaint = nvgImagePattern(vg, spinner_img_x, spinner_img_y,
      spinner_img_s, spinner_img_s, 0, spinner_comma_img, 1.0f);
    nvgBeginPath(vg);
    nvgFillPaint(vg, comma_imgPaint);
    nvgRect(vg, spinner_img_x, spinner_img_y, spinner_img_s, spinner_img_s);
    nvgFill(vg);

    if (draw_progress){
      // draw progress bar
      int progress_width = 1000;
      int progress_x = fb_w/2-progress_width/2;
      int progress_y = 775;
      int progress_height = 25;

      NVGpaint paint = nvgBoxGradient(
          vg, progress_x + 1, progress_y + 1,
          progress_width - 2, progress_height, 3, 4, nvgRGB(27, 27, 27), nvgRGB(27, 27, 27));
      nvgBeginPath(vg);
      nvgRoundedRect(vg, progress_x, progress_y, progress_width, progress_height, 12);
      nvgFillPaint(vg, paint);
      nvgFill(vg);

      int bar_pos = ((progress_width - 2) * progress_val);

      paint = nvgBoxGradient(
          vg, progress_x, progress_y,
          bar_pos+1.5f, progress_height-1, 3, 4,
          nvgRGB(245, 245, 245), nvgRGB(105, 105, 105));

      nvgBeginPath(vg);
      nvgRoundedRect(
          vg, progress_x+1, progress_y+1,
          bar_pos, progress_height-2, 12);
      nvgFillPaint(vg, paint);
      nvgFill(vg);
    }

    if (!draw_progress) {
      // message
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
      nvgFontSize(vg, 96.0f);
      nvgText(vg, fb_w/2, (fb_h*2/3)+24, spintext, NULL);
    } else if (has_extra) {
      nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
      if (err_msg) {
        int break_row_width = 1300;
        int y_offset = strlen(spinerr) > 160 ? 76 : 96;
        // need smaller font for longer error msg
        int fontsize = strlen(spinerr) > 50 ? 68.0f : 72.0f;
        fontsize = strlen(spinerr) > 70 ? 62.0f : fontsize;
        fontsize = strlen(spinerr) > 120 ? 59.0f : fontsize;

        nvgFontSize(vg, fontsize);
        nvgTextBox(vg, (fb_w/2)-(break_row_width/2), (fb_h*2/3)+24+y_offset, break_row_width, spinerr, NULL);
        // nvgText(vg, fb_w/2, (fb_h*2/3)+24+y_offset, "line1\nline2", NULL);
        printf("spinerr: %s\n", spinerr);
      } else {
        nvgFontSize(vg, 78.0f);
        nvgText(vg, fb_w/2, (fb_h*2/3)+24+96, spinstatus, NULL);
      }
    }

    nvgEndFrame(vg);
    framebuffer_swap(fb);
    assert(glGetError() == GL_NO_ERROR);
  }

  return 0;
}
