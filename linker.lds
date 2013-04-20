SECTIONS {
  __audio_frontends_begin = ADDR(audio_frontends);
  __audio_frontends_end = __audio_frontends_begin + SIZEOF(audio_frontends);
  __input_frontends_begin = ADDR(input_frontends);
  __input_frontends_end = __input_frontends_begin + SIZEOF(input_frontends);
  __video_frontends_begin = ADDR(video_frontends);
  __video_frontends_end = __video_frontends_begin + SIZEOF(video_frontends);
  __machines_begin = ADDR(machines);
  __machines_end = __machines_begin + SIZEOF(machines);
  __cpus_begin = ADDR(cpus);
  __cpus_end = __cpus_begin + SIZEOF(cpus);
  __controllers_begin = ADDR(controllers);
  __controllers_end = __controllers_begin + SIZEOF(controllers);
}
INSERT AFTER .text;

