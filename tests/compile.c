#include <dr_libs/dr_flac.h>
#include <dr_libs/dr_mp3.h>
#include <dr_libs/dr_wav.h>

int main(int argc, char** argv) {
  drflac_open_file("", NULL);
  drwav_init_file("", NULL);
  drmp3_init_file("", NULL);
  (void)argv;
  return 0;
}
