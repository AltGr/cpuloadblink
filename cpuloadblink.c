#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stdlib.h>

const char* led_trigger_path = "/sys/class/leds/led0/trigger";
const char* led_control_path = "/sys/class/leds/led0/brightness";
const char* procstat = "/proc/stat";

const int base_delay   = 1000000; /* 1 second */
const int min_delay    =  100000; /* 0.1 seconds */
const int update_delay =  500000; /* 0.5 seconds */

void blink(int led_fd) {
  static int onoff = 0;
  lseek(led_fd, 0, SEEK_SET);
  write(led_fd, onoff ? "0\n" : "1\n", 2);
  onoff = !onoff;
}

#define MAX_STAT_SIZE 256

int read_stat(int stat_fd) {
  static int last_idle, last_total = 0;
  static char stat_read[MAX_STAT_SIZE+1];
  int nread;
  int user, nice, system, idle;
  int total, idle_percent;

  lseek(stat_fd, 0, SEEK_SET);
  nread = read(stat_fd, stat_read, MAX_STAT_SIZE);
  if (nread < 0) return 100;
  stat_read[nread] = '\0';
  sscanf(stat_read, "cpu %d %d %d %d", &user, &nice, &system, &idle);
  total = user + nice + system + idle;
  if (total == last_total) return 0;
  idle_percent = 100 * (idle - last_idle) / (total - last_total);

  last_idle = idle;
  last_total = total;
  return idle_percent;
}

int main (int argc, char** argv) {
  int stat_fd;
  int led_fd;
  int led_trigger_fd;

  int idle;
  int blink_delay = base_delay;
  int tblink = blink_delay; /* time until next blink */
  int tupdate = update_delay; /* time until next update */

  /* Disable led triggers to get us free access to the led */
  led_trigger_fd = open(led_trigger_path, O_WRONLY);
  assert(led_trigger_fd >= 0);
  write(led_trigger_fd, "none\n", 5);
  close(led_trigger_fd);

  stat_fd = open(procstat, O_RDONLY);
  assert(stat_fd >= 0);

  led_fd = open(led_control_path, O_WRONLY|O_TRUNC);
  assert(led_fd >= 0);

  while(1) {
    idle = read_stat(stat_fd);
    fflush(stdout);
    tblink -= blink_delay;
    blink_delay = (min_delay * 100 + (base_delay - min_delay) * idle) / 100;
    tblink += blink_delay;
    tblink = tblink < 0 ? 0 : tblink;
    while (tblink < tupdate) {
      usleep(tblink);
      tupdate -= tblink;
      tblink = blink_delay;
      blink(led_fd);
    }
    usleep (tupdate);
    tblink -= tupdate;
    tupdate = update_delay;
  }
}
